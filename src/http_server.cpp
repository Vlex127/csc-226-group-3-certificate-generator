#include "http_server.hpp"
#include "exceptions.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

// ── HTTP Helpers ─────────────────────────────────────────────────────────

// Decode URL-encoded strings: %XX hex codes and + for space
static std::string urlDecode(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); i++) {
        if (src[i] == '+') { out += ' '; }
        else if (src[i] == '%' && i + 2 < src.size()) {
            auto hex = src.substr(i + 1, 2);
            out += static_cast<char>(std::stoi(hex, nullptr, 16));
            i += 2;
        } else {
            out += src[i];
        }
    }
    return out;
}

// Parse application/x-www-form-urlencoded POST body into a key-value map
static std::map<std::string, std::string> parseForm(const std::string& body) {
    std::map<std::string, std::string> fields;
    std::stringstream ss(body);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq == std::string::npos) continue;
        auto key = urlDecode(pair.substr(0, eq));
        auto val = urlDecode(pair.substr(eq + 1));
        fields[key] = val;
    }
    return fields;
}

// Escape a string for safe inclusion in JSON (quotes, backslashes, control chars)
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        if (c == '"')       out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

// ── Embedded Web UI ──────────────────────────────────────────────────────
// This raw string literal contains the complete HTML/CSS/JS frontend.
// It is served when the browser requests GET / and runs entirely client-side.
// The JavaScript uses fetch() to call back to the C++ API endpoints below.

static const char* UI_HTML = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Certificate Generator</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f0f2f5;min-height:100vh}
  .header{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;padding:20px 40px;display:flex;align-items:center;justify-content:space-between;box-shadow:0 2px 10px rgba(0,0,0,.15)}
  .header h1{font-size:22px;font-weight:700;letter-spacing:.5px}
  .header .badge{background:rgba(255,255,255,.2);border-radius:12px;padding:4px 14px;font-size:13px}
  .tabs{background:#fff;padding:0 40px;display:flex;gap:0;border-bottom:2px solid #e8e8e8}
  .tabs button{background:none;border:none;padding:14px 20px;cursor:pointer;font-size:14px;color:#666;border-bottom:3px solid transparent;transition:all .2s}
  .tabs button:hover{color:#333}
  .tabs button.active{color:#667eea;border-bottom-color:#667eea;font-weight:600}
  .content{max-width:1100px;margin:0 auto;padding:30px 40px}
  .card{background:#fff;border-radius:12px;padding:30px;box-shadow:0 1px 4px rgba(0,0,0,.06);margin-bottom:24px}
  .card h2{font-size:18px;color:#333;margin-bottom:16px}
  .card h3{font-size:15px;color:#555;margin-bottom:12px}
  .form-row{display:flex;gap:12px;flex-wrap:wrap;align-items:end}
  .form-row > div{flex:1;min-width:180px}
  label{display:block;font-size:12px;color:#888;margin-bottom:4px;text-transform:uppercase;letter-spacing:.5px}
  input,select{width:100%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;font-size:14px;outline:none;transition:border .2s}
  input:focus,select:focus{border-color:#667eea}
  .btn{background:#667eea;color:#fff;border:none;padding:10px 24px;border-radius:8px;cursor:pointer;font-size:14px;transition:background .2s}
  .btn:hover{background:#5a6fd6}
  .btn-sm{padding:6px 14px;font-size:12px}
  .btn-danger{background:#e74c3c}
  .btn-danger:hover{background:#c0392b}
  .btn-success{background:#27ae60}
  .btn-success:hover{background:#219a52}
  .btn-outline{background:transparent;color:#667eea;border:1px solid #667eea}
  .btn-outline:hover{background:#667eea;color:#fff}
  table{width:100%;border-collapse:collapse;font-size:14px}
  th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #eee}
  th{color:#888;font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:.5px}
  tr:hover{background:#f8f9ff}
  .empty{text-align:center;color:#aaa;padding:40px;font-size:14px}
  .msg{padding:12px 16px;border-radius:8px;margin-bottom:16px;font-size:14px;display:none}
  .msg.success{display:block;background:#eafaf1;color:#27ae60;border:1px solid #d5f5e3}
  .msg.error{display:block;background:#fdedec;color:#e74c3c;border:1px solid #fadbd8}
  .stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px}
  .stat-card{background:#f8f9ff;border-radius:10px;padding:20px;text-align:center}
  .stat-card .num{font-size:32px;font-weight:700;color:#667eea}
  .stat-card .label{font-size:13px;color:#888;margin-top:4px}
  .grade-dist{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
  .grade-dist span{background:#667eea;color:#fff;padding:6px 14px;border-radius:6px;font-size:13px}
  .search-bar{display:flex;gap:10px;margin-bottom:16px}
  .search-bar input{flex:1}
  .tab-panel{display:none}
  .tab-panel.active{display:block}
  .inline-flex{display:flex;gap:8px;align-items:center}
  pre{background:#f5f5f5;padding:12px;border-radius:6px;font-size:12px;overflow-x:auto}
</style>
</head>
<body>

<div class="header">
  <h1>&#x1F4DC; Certificate Generator</h1>
  <span class="badge" id="studentBadge">0 students</span>
</div>

<div class="tabs" id="tabs">
  <button class="active" data-tab="dashboard">Dashboard</button>
  <button data-tab="students">Students</button>
  <button data-tab="add">Add Student</button>
  <button data-tab="csv">Load CSV</button>
  <button data-tab="generate">Generate</button>
  <button data-tab="stats">Statistics</button>
</div>

<div class="content">

<div id="msg" class="msg"></div>

<!-- Dashboard tab -->
<div class="tab-panel active" id="panel-dashboard">
  <div class="card">
    <h2>Dashboard</h2>
    <div class="stats-grid" id="dashboardStats">
      <div class="stat-card"><div class="num" id="dashCount">0</div><div class="label">Students</div></div>
      <div class="stat-card"><div class="num" id="dashAvg">-</div><div class="label">Avg GPA</div></div>
      <div class="stat-card"><div class="num" id="dashCourses">0</div><div class="label">Courses</div></div>
    </div>
  </div>
  <div class="card">
    <h2>Quick Actions</h2>
    <div style="display:flex;gap:10px;flex-wrap:wrap">
      <button class="btn" onclick="switchTab('add')">&#x2795; Add Student</button>
      <button class="btn btn-outline" onclick="switchTab('csv')">&#x1F4C2; Load CSV</button>
      <button class="btn btn-success" onclick="switchTab('generate')">&#x1F393; Generate</button>
    </div>
  </div>
</div>

<!-- Students tab — searchable, sortable table with delete -->
<div class="tab-panel" id="panel-students">
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:16px">
      <h2 style="margin:0">All Students</h2>
      <div class="inline-flex">
        <button class="btn btn-sm btn-outline" onclick="sortStudents('name')">Sort A-Z</button>
        <button class="btn btn-sm btn-outline" onclick="sortStudents('grade_desc')">Grade</button>
        <button class="btn btn-sm btn-outline" onclick="sortStudents('course')">Course</button>
      </div>
    </div>
    <div class="search-bar">
      <input type="text" id="searchInput" placeholder="Search by name or course..." oninput="searchStudents()">
      <button class="btn btn-sm" onclick="searchStudents()">Search</button>
    </div>
    <div id="studentTableWrap">
      <div class="empty">No students loaded. Add one or load from a CSV file.</div>
    </div>
  </div>
</div>

<!-- Add Student tab -->
<div class="tab-panel" id="panel-add">
  <div class="card">
    <h2>Add Student</h2>
    <div class="form-row">
      <div><label>Full Name</label><input type="text" id="addName" placeholder="e.g. John Doe"></div>
      <div><label>Course</label><input type="text" id="addCourse" placeholder="e.g. CSC 226"></div>
      <div><label>Grade</label><input type="text" id="addGrade" placeholder="e.g. A, B+, 93"></div>
      <div><button class="btn" onclick="addStudent()">Add</button></div>
    </div>
  </div>
</div>

<!-- Load CSV tab -->
<div class="tab-panel" id="panel-csv">
  <div class="card">
    <h2>Load Students from CSV</h2>
    <p style="color:#888;font-size:13px;margin-bottom:16px">Format: <code>Name,Course,Grade</code> (one per line)</p>
    <div class="form-row">
      <div style="flex:2"><label>Filename</label><input type="text" id="csvFile" value="students.csv" placeholder="students.csv"></div>
      <div><button class="btn" onclick="loadCsv()">Load</button></div>
    </div>
  </div>
</div>

<!-- Generate tab -->
<div class="tab-panel" id="panel-generate">
  <div class="card">
    <h2>Generate Certificates</h2>
    <div class="form-row">
      <div><label>Student Name (leave blank for all)</label><input type="text" id="genName" placeholder="All students if empty"></div>
      <div><label>Certificate Type</label>
        <select id="genType">
          <option value="0">Excellence</option>
          <option value="1">Completion</option>
          <option value="2">Participation</option>
        </select>
      </div>
      <div><label>Template Style</label>
        <select id="genStyle">
          <option value="0">Formal</option>
          <option value="1">Modern</option>
          <option value="2">Minimalist</option>
        </select>
      </div>
      <div><button class="btn btn-success" onclick="generate()">Generate</button></div>
    </div>
  </div>
</div>

<!-- Statistics tab -->
<div class="tab-panel" id="panel-stats">
  <div class="card">
    <h2>Statistics</h2>
    <div id="statsContent">
      <div class="empty">No data to show.</div>
    </div>
  </div>
</div>

</div>

<script>
const API = '';
let currentTab = 'dashboard';

function showMsg(msg, type) {
  const el = document.getElementById('msg');
  el.textContent = msg;
  el.className = 'msg ' + type;
  setTimeout(() => { el.className = 'msg'; }, 4000);
}

function switchTab(name) {
  currentTab = name;
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tabs button').forEach(b => b.classList.remove('active'));
  document.getElementById('panel-' + name).classList.add('active');
  document.querySelector('.tabs button[data-tab="' + name + '"]').classList.add('active');
  if (name === 'students') refreshTable();
  if (name === 'stats') loadStats();
  if (name === 'dashboard') loadDashboard();
}

document.getElementById('tabs').addEventListener('click', function(e) {
  if (e.target.tagName === 'BUTTON') switchTab(e.target.dataset.tab);
});

// Generic fetch wrapper — sends requests to the C++ backend
async function api(method, path, body) {
  const opts = { method };
  if (body) {
    opts.headers = { 'Content-Type': 'application/x-www-form-urlencoded' };
    opts.body = body;
  }
  const res = await fetch(API + path, opts);
  return res.json();
}

// Dashboard tab — load counts from /list
async function loadDashboard() {
  try {
    const r = await api('GET', '/list');
    const students = r.data || [];
    document.getElementById('dashCount').textContent = students.length;
    if (students.length > 0) {
      const avg = students.reduce(function(s, st) { return s + (st.gpa || 0); }, 0) / students.length;
      document.getElementById('dashAvg').textContent = avg.toFixed(2);
      var courses = new Set(students.map(function(s) { return s.course; }));
      document.getElementById('dashCourses').textContent = courses.size;
    } else {
      document.getElementById('dashAvg').textContent = '-';
      document.getElementById('dashCourses').textContent = '0';
    }
    document.getElementById('studentBadge').textContent = students.length + ' student' + (students.length !== 1 ? 's' : '');
  } catch(e) { showMsg('Failed to load dashboard', 'error'); }
}

// Students tab — render the table from /list or /search
async function refreshTable(query, type) {
  try {
    var url = '/list';
    if (query) url = '/search?q=' + encodeURIComponent(query) + '&type=' + (type || 'name');
    const r = await api('GET', url);
    const students = r.data || [];
    const wrap = document.getElementById('studentTableWrap');
    if (students.length === 0) {
      wrap.innerHTML = '<div class="empty">' + (query ? 'No matching students.' : 'No students loaded.') + '</div>';
      return;
    }
    var html = '<table><thead><tr><th>Name</th><th>Course</th><th>Grade</th><th>GPA</th><th></th></tr></thead><tbody>';
    students.forEach(function(s) {
      html += '<tr><td>' + esc(s.name) + '</td><td>' + esc(s.course) + '</td><td>' + esc(s.grade) + '</td><td>' + (s.gpa || 0).toFixed(1) + '</td>';
      html += '<td><button class="btn btn-sm btn-danger" onclick="removeStudent(\'' + esc(s.name) + '\')">X</button></td></tr>';
    });
    html += '</tbody></table>';
    wrap.innerHTML = html;
    document.getElementById('studentBadge').textContent = students.length + ' student' + (students.length !== 1 ? 's' : '');
  } catch(e) { showMsg('Failed to load students', 'error'); }
}

function searchStudents() {
  var q = document.getElementById('searchInput').value.trim();
  if (!q) { refreshTable(); return; }
  refreshTable(q, 'name');
}

async function sortStudents(by) {
  try {
    await api('POST', '/sort', 'by=' + encodeURIComponent(by));
    refreshTable();
  } catch(e) { showMsg('Sort failed', 'error'); }
}

function esc(s) { return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/'/g,'&#39;').replace(/"/g,'&quot;'); }

// Add Student form handler
async function addStudent() {
  var name = document.getElementById('addName').value.trim();
  var course = document.getElementById('addCourse').value.trim();
  var grade = document.getElementById('addGrade').value.trim();
  if (!name || !course || !grade) { showMsg('Fill all fields', 'error'); return; }
  try {
    const r = await api('POST', '/add', 'name=' + encodeURIComponent(name) + '&course=' + encodeURIComponent(course) + '&grade=' + encodeURIComponent(grade));
    if (r.status === 'ok') {
      showMsg('Added: ' + name, 'success');
      document.getElementById('addName').value = '';
      document.getElementById('addCourse').value = '';
      document.getElementById('addGrade').value = '';
      loadDashboard();
    } else {
      showMsg(r.message || 'Failed', 'error');
    }
  } catch(e) { showMsg('Error adding student', 'error'); }
}

// Remove student with confirmation
async function removeStudent(name) {
  if (!confirm('Remove "' + name + '"?')) return;
  try {
    const r = await api('POST', '/remove', 'name=' + encodeURIComponent(name));
    if (r.status === 'ok') {
      showMsg('Removed: ' + name, 'success');
      refreshTable();
      loadDashboard();
    } else {
      showMsg(r.message || 'Not found', 'error');
    }
  } catch(e) { showMsg('Error removing student', 'error'); }
}

// CSV load handler
async function loadCsv() {
  var file = document.getElementById('csvFile').value.trim();
  if (!file) { showMsg('Enter a filename', 'error'); return; }
  try {
    const r = await api('POST', '/load', 'filename=' + encodeURIComponent(file));
    showMsg(r.message || 'Loaded', r.status === 'ok' ? 'success' : 'error');
    refreshTable();
    loadDashboard();
  } catch(e) { showMsg('Error loading file', 'error'); }
}

// Certificate generation handler
async function generate() {
  var name = document.getElementById('genName').value.trim();
  var certType = document.getElementById('genType').value;
  var style = document.getElementById('genStyle').value;
  var body = 'certType=' + certType + '&style=' + style;
  if (name) body += '&name=' + encodeURIComponent(name);
  try {
    const r = await api('POST', '/generate', body);
    showMsg(r.message || 'Done!', r.status === 'ok' ? 'success' : 'error');
  } catch(e) { showMsg('Error generating', 'error'); }
}

// Statistics tab — fetch stats from /stats and render cards
async function loadStats() {
  try {
    const r = await api('GET', '/stats');
    if (r.status !== 'ok') {
      document.getElementById('statsContent').innerHTML = '<div class="empty">No data.</div>';
      return;
    }
    var d = r.data;
    var html = '<div class="stats-grid">';
    html += '<div class="stat-card"><div class="num">' + d.total + '</div><div class="label">Total Students</div></div>';
    html += '<div class="stat-card"><div class="num">' + (d.avgGpa || 0).toFixed(2) + '</div><div class="label">Average GPA</div></div>';
    html += '<div class="stat-card"><div class="num">' + Object.keys(d.courses || {}).length + '</div><div class="label">Courses</div></div>';
    html += '</div>';

    if (d.courses && Object.keys(d.courses).length) {
      html += '<h3 style="margin-top:24px">Per Course</h3><table><thead><tr><th>Course</th><th>Count</th></tr></thead><tbody>';
      for (var course in d.courses) {
        html += '<tr><td>' + esc(course) + '</td><td>' + d.courses[course] + '</td></tr>';
      }
      html += '</tbody></table>';
    }

    if (d.grades && Object.keys(d.grades).length) {
      html += '<h3 style="margin-top:24px">Grade Distribution</h3><div class="grade-dist">';
      for (var grade in d.grades) {
        html += '<span>' + esc(grade) + ': ' + d.grades[grade] + '</span>';
      }
      html += '</div>';
    }
    document.getElementById('statsContent').innerHTML = html;
  } catch(e) { showMsg('Error loading stats', 'error'); }
}

// Initialise on page load
loadDashboard();
refreshTable();
</script>
</body>
</html>
)html";

// ── HTTP Server Constructor ──────────────────────────────────────────────

HttpServer::HttpServer(CertificateGenerator& gen, int port)
    : gen_(gen), port_(port) {}

// ── HTTP Request Handler ─────────────────────────────────────────────────
// Called in a detached thread for each incoming connection.
// Parses the raw HTTP request, dispatches to the correct route,
// and sends back a JSON or HTML response.

static void handleClient(SOCKET client, CertificateGenerator& gen) {
    std::string request;
    char buf[4096];
    int received;

    // Read request until headers end (\r\n\r\n)
    do {
        received = recv(client, buf, sizeof(buf) - 1, 0);
        if (received > 0) {
            buf[received] = 0;
            request += buf;
        }
    } while (received > 0 && request.find("\r\n\r\n") == std::string::npos);

    // Lambda to send a complete HTTP response
    auto sendResponse = [&](int status, const std::string& ct, const std::string& body) {
        std::string statusText = (status == 200) ? "OK" : (status == 404) ? "Not Found" : "Error";
        std::ostringstream res;
        res << "HTTP/1.1 " << status << " " << statusText << "\r\n"
            << "Content-Type: " << ct << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Access-Control-Allow-Origin: *\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << body;
        send(client, res.str().data(), static_cast<int>(res.str().size()), 0);
    };

    auto sendJSON = [&](int status, const std::string& json) {
        sendResponse(status, "application/json", json);
    };

    auto sendHTML = [&](const std::string& html) {
        sendResponse(200, "text/html", html);
    };

    // Parse HTTP method, path, and body from raw request
    std::string method, path, body;
    std::stringstream ss(request);
    ss >> method >> path;
    auto bodyPos = request.find("\r\n\r\n");
    if (bodyPos != std::string::npos && bodyPos + 4 < request.size()) {
        body = request.substr(bodyPos + 4);
    }

    // ── Route Dispatch ───────────────────────────────────────────────
    try {
        // GET / — serve the embedded web UI
        if (method == "GET" && path == "/") {
            sendHTML(UI_HTML);
        }
        // GET /list — return all students as JSON array
        else if (method == "GET" && path == "/list") {
            auto students = gen.getAllStudents();
            std::string json = R"({"status":"ok","data":[)";
            for (size_t i = 0; i < students.size(); i++) {
                if (i > 0) json += ",";
                json += R"({"name":")" + jsonEscape(students[i].getName()) + "\",";
                json += R"("course":")" + jsonEscape(students[i].getCourse()) + "\",";
                json += R"("grade":")" + jsonEscape(students[i].getGrade()) + "\",";
                json += R"("gpa":)" + std::to_string(students[i].gradeToPoints()) + "}";
            }
            json += "]}";
            sendJSON(200, json);
        }
        // GET /search?q=...&type=name|course — search and return matching students as JSON
        else if (method == "GET" && path.find("/search") == 0) {
            auto qpos = path.find("q=");
            auto tpos = path.find("type=");
            std::string q, type = "name";
            if (qpos != std::string::npos) {
                auto end = path.find('&', qpos);
                q = urlDecode(path.substr(qpos + 2, end - qpos - 2));
            }
            if (tpos != std::string::npos) {
                type = path.substr(tpos + 5);
                auto amp = type.find('&');
                if (amp != std::string::npos) type = type.substr(0, amp);
            }

            std::vector<Student> results;
            if (type == "course") {
                results = gen.searchByCourse(q);
            } else {
                results = gen.searchByName(q);
            }

            std::string json = R"({"status":"ok","data":[)";
            for (size_t i = 0; i < results.size(); i++) {
                if (i > 0) json += ",";
                json += R"({"name":")" + jsonEscape(results[i].getName()) + "\",";
                json += R"("course":")" + jsonEscape(results[i].getCourse()) + "\",";
                json += R"("grade":")" + jsonEscape(results[i].getGrade()) + "\",";
                json += R"("gpa":)" + std::to_string(results[i].gradeToPoints()) + "}";
            }
            json += "]}";
            sendJSON(200, json);
        }
        // GET /stats — return computed statistics as JSON
        else if (method == "GET" && path == "/stats") {
            auto stats = gen.computeStats();
            std::string json = std::string(R"({"status":"ok","data":{)")
                + R"("total":)" + std::to_string(stats.total) + ","
                + R"("avgGpa":)" + std::to_string(stats.avgGradePoints) + ","
                + R"("courses":{)";
            bool first = true;
            for (const auto& [c, n] : stats.courseCounts) {
                if (!first) json += ",";
                json += "\"" + jsonEscape(c) + "\":" + std::to_string(n);
                first = false;
            }
            json += "},\"grades\":{";
            first = true;
            for (const auto& [g, n] : stats.gradeDistribution) {
                if (!first) json += ",";
                json += "\"" + jsonEscape(g) + "\":" + std::to_string(n);
                first = false;
            }
            json += "}}}";
            sendJSON(200, json);
        }
        // POST /add — add a student from form fields
        else if (method == "POST" && path == "/add") {
            auto fields = parseForm(body);
            Student s(fields["name"], fields["course"], fields["grade"]);
            gen.addStudent(s);
            sendJSON(200, R"({"status":"ok","message":"Added ")" + jsonEscape(fields["name"]) + "\"}");
        }
        // POST /remove — remove a student by name
        else if (method == "POST" && path == "/remove") {
            auto fields = parseForm(body);
            if (gen.removeStudent(fields["name"])) {
                sendJSON(200, R"({"status":"ok","message":"Removed"})");
            } else {
                sendJSON(200, R"({"status":"error","message":"Student not found"})");
            }
        }
        // POST /load — batch load from CSV file
        else if (method == "POST" && path == "/load") {
            auto fields = parseForm(body);
            try {
                gen.loadFromCsv(fields["filename"]);
                sendJSON(200, R"({"status":"ok","message":"Loaded from )" + jsonEscape(fields["filename"]) + "\"}");
            } catch (const FileException& e) {
                sendJSON(200, R"({"status":"error","message":")" + jsonEscape(e.what()) + "\"}");
            }
        }
        // POST /generate — generate certificate(s) with specified type and style
        else if (method == "POST" && path == "/generate") {
            auto fields = parseForm(body);
            int certType = 1, style = 0;
            try { certType = std::stoi(fields["certType"]); } catch (...) {}
            try { style = std::stoi(fields["style"]); } catch (...) {}

            auto ct = static_cast<CertType>(certType);
            auto st = static_cast<TemplateStyle>(style);

            if (fields.count("name") && !fields["name"].empty()) {
                gen.generateCertificateFor(fields["name"], ct, st);
            } else {
                gen.generateAllCertificates(ct, st);
            }
            sendJSON(200, R"({"status":"ok","message":"Certificates generated"})");
        }
        // POST /sort — sort the student list
        else if (method == "POST" && path == "/sort") {
            auto fields = parseForm(body);
            auto by = fields["by"];
            if (by == "name")       gen.sortByName();
            else if (by == "grade_desc") gen.sortByGrade(true);
            else if (by == "grade_asc")  gen.sortByGrade(false);
            else if (by == "course")     gen.sortByCourse();
            sendJSON(200, R"({"status":"ok","message":"Sorted"})");
        }
        // Catch-all: 404
        else {
            sendJSON(404, R"({"status":"error","message":"Not found"})");
        }
    }
    catch (const ValidationException& e) {
        std::string json = R"({"status":"error","message":")" + jsonEscape(e.what()) + "\"}";
        sendJSON(200, json);
    }
    catch (const std::exception& e) {
        std::string json = R"({"status":"error","message":"Server error: )" + jsonEscape(e.what()) + "\"}";
        sendJSON(500, json);
    }

    shutdown(client, SD_SEND);
    closesocket(client);
}

// ── Server Entry Point ───────────────────────────────────────────────────

void HttpServer::start() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return;
    }

    // Create TCP socket
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return;
    }

    // Allow immediate address reuse after restart
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&opt), sizeof(opt));

    // Bind to localhost on the specified port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Bind failed on port " << port_
                  << ". Try a different port." << std::endl;
        closesocket(server);
        WSACleanup();
        return;
    }

    listen(server, SOMAXCONN);

    // Print status and open browser
    std::string url = "http://localhost:" + std::to_string(port_);
    std::cout << "\n  ========================================\n";
    std::cout << "   Certificate Generator Web UI\n";
    std::cout << "  ========================================\n";
    std::cout << "   Server running at: " << url << "\n";
    std::cout << "   Opening in your browser...\n";
    std::cout << "  ========================================\n";
    std::cout << "   Press Ctrl+C to stop.\n\n" << std::flush;

    // Open the default browser to the server URL
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    // Accept loop — each connection gets a detached thread
    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        std::thread(handleClient, client, std::ref(gen_)).detach();
    }

    closesocket(server);
    WSACleanup();
}
