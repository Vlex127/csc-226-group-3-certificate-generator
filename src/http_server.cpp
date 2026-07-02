#include "http_server.hpp"
#include "exceptions.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

// Security: limit request size to prevent memory exhaustion
static const size_t MAX_REQUEST_SIZE = 64 * 1024;

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
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LASU Certificate Generator | Dept. of Computer Science</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
:root {
  --bg-primary: #faf9f5;
  --bg-secondary: #F4F3EE;
  --bg-tertiary: #e8e6dc;
  --bg-elevated: #d1cfc5;
  --border: #e8e6dc;
  --border-hover: #b0aea5;
  --text-primary: #141413;
  --text-secondary: #4a4a47;
  --text-muted: #b0aea5;
  --accent: #d97757;
  --accent-hover: #c86645;
  --accent-dim: rgba(217, 119, 87, 0.15);
  --secondary: #6a9bcc;
  --secondary-dim: rgba(106, 155, 204, 0.15);
  --success: #788c5d;
  --success-dim: rgba(120, 140, 93, 0.15);
  --warning: #c89d3c;
  --warning-dim: rgba(200, 157, 60, 0.15);
  --error: #c0392b;
  --error-dim: rgba(192, 57, 43, 0.15);
  --lasu-green: #788c5d;
  --lasu-gold: #d97757;
  --radius-sm: 6px;
  --radius: 8px;
  --radius-lg: 12px;
  --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.08);
  --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
  --transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
}
[data-theme="dark"] {
  --bg-primary: #141413;
  --bg-secondary: #1a1a18;
  --bg-tertiary: #2a2a27;
  --bg-elevated: #3a3a37;
  --border: #2a2a27;
  --border-hover: #3a3a37;
  --text-primary: #faf9f5;
  --text-secondary: #b0aea5;
  --text-muted: #6a6a65;
  --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.4);
  --shadow-lg: 0 10px 15px -3px rgba(0, 0, 0, 0.5);
}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:Inter,-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;background:var(--bg-primary);color:var(--text-primary);min-height:100vh;line-height:1.6}
::-webkit-scrollbar{width:8px;height:8px}
::-webkit-scrollbar-track{background:var(--bg-secondary)}
::-webkit-scrollbar-thumb{background:var(--bg-elevated);border-radius:4px}
::-webkit-scrollbar-thumb:hover{background:var(--border-hover)}
.app{display:flex;flex-direction:column;min-height:100vh}
.header{background:linear-gradient(180deg,var(--bg-secondary) 0%,var(--bg-primary) 100%);border-bottom:1px solid var(--border);padding:20px 32px;position:sticky;top:0;z-index:100;backdrop-filter:blur(12px)}
.header-content{max-width:1400px;margin:0 auto;display:flex;align-items:center;justify-content:space-between;gap:24px}
.logo-section{display:flex;align-items:center;gap:16px}
.logo{width:56px;height:56px;border-radius:var(--radius);object-fit:contain;background:white;padding:4px;box-shadow:var(--shadow)}
.brand{display:flex;flex-direction:column}
.brand-title{font-size:20px;font-weight:700;color:var(--text-primary);letter-spacing:-0.3px}
.brand-subtitle{font-size:12px;color:var(--text-muted);text-transform:uppercase;letter-spacing:1px}
.brand-subtitle span{color:var(--accent)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.theme-toggle{display:flex;gap:2px;background:var(--bg-tertiary);padding:3px;border-radius:var(--radius)}
.theme-btn{display:flex;align-items:center;justify-content:center;width:32px;height:32px;border:none;background:transparent;color:var(--text-muted);border-radius:var(--radius-sm);cursor:pointer;transition:var(--transition)}
.theme-btn:hover{color:var(--text-primary)}
.theme-btn.active{background:var(--bg-primary);color:var(--accent);box-shadow:var(--shadow)}
.main{flex:1;padding:32px;max-width:1400px;margin:0 auto;width:100%}
.tabs{display:flex;gap:4px;background:var(--bg-secondary);padding:4px;border-radius:var(--radius-lg);margin-bottom:32px;width:fit-content}
.tab{padding:10px 20px;border:none;background:transparent;color:var(--text-secondary);font-size:14px;font-weight:500;border-radius:var(--radius);cursor:pointer;transition:var(--transition)}
.tab:hover{color:var(--text-primary);background:var(--bg-tertiary)}
.tab.active{background:var(--accent);color:white}
.card{background:var(--bg-secondary);border:1px solid var(--border);border-radius:var(--radius-lg);padding:24px;margin-bottom:24px;transition:var(--transition)}
.card:hover{border-color:var(--border-hover)}
.card-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px}
.card-title{font-size:16px;font-weight:600;color:var(--text-primary);display:flex;align-items:center;gap:10px}
.card-title svg{width:20px;height:20px;color:var(--accent)}
.form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}
.form-group{display:flex;flex-direction:column;gap:6px}
label{font-size:13px;font-weight:500;color:var(--text-secondary)}
input,select{padding:12px 14px;background:var(--bg-primary);border:1px solid var(--border);border-radius:var(--radius);color:var(--text-primary);font-size:14px;font-family:inherit;transition:var(--transition)}
input:focus,select:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 3px var(--accent-dim)}
input::placeholder{color:var(--text-muted)}
select{cursor:pointer;appearance:none;background-image:url("data:image/svg+xml,%3Csvg xmlns=%27http://www.w3.org/2000/svg%27 width=%2716%27 height=%2716%27 viewBox=%270 0 24 24%27 fill=%27none%27 stroke=%27%23a1a1aa%27 stroke-width=%272%27%3E%3Cpath d=%27M6 9l6 6 6-6%27/%3E%3C/svg%3E");background-repeat:no-repeat;background-position:right 12px center;padding-right:40px}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;padding:12px 20px;border:none;border-radius:var(--radius);font-size:14px;font-weight:500;font-family:inherit;cursor:pointer;transition:var(--transition)}
.btn-primary{background:var(--accent);color:white}
.btn-primary:hover{background:var(--accent-hover);transform:translateY(-1px);box-shadow:0 4px 12px rgba(217,119,87,0.35)}
.btn-secondary{background:var(--bg-tertiary);color:var(--text-primary)}
.btn-secondary:hover{background:var(--bg-elevated)}
.btn-danger{background:var(--error-dim);color:var(--error)}
.btn-danger:hover{background:var(--error);color:white}
.btn-success{background:var(--success-dim);color:var(--success)}
.btn-success:hover{background:var(--success);color:white}
.btn-group{display:flex;gap:12px;flex-wrap:wrap;margin-top:20px}
.table-container{overflow-x:auto;border-radius:var(--radius);border:1px solid var(--border)}
table{width:100%;border-collapse:collapse;font-size:14px}
th{text-align:left;padding:14px 16px;background:var(--bg-tertiary);color:var(--text-secondary);font-weight:600;font-size:12px;text-transform:uppercase;letter-spacing:0.5px;border-bottom:1px solid var(--border)}
td{padding:14px 16px;border-bottom:1px solid var(--border);color:var(--text-primary)}
tr:last-child td{border-bottom:none}
tr:hover td{background:var(--bg-tertiary)}
.grade-badge{display:inline-block;padding:4px 10px;border-radius:12px;font-size:12px;font-weight:600;font-family:JetBrains Mono,monospace}
.grade-a{background:var(--success-dim);color:var(--success)}
.grade-b{background:var(--secondary-dim);color:var(--secondary)}
.grade-c{background:var(--warning-dim);color:var(--warning)}
.grade-f{background:var(--error-dim);color:var(--error)}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:24px}
.stat-card{background:var(--bg-secondary);border:1px solid var(--border);border-radius:var(--radius-lg);padding:20px}
.stat-label{font-size:13px;color:var(--text-muted);margin-bottom:8px}
.stat-value{font-size:32px;font-weight:700;color:var(--text-primary)}
.stat-value.accent{color:var(--accent)}
.stat-value.success{color:var(--success)}
.toast-container{position:fixed;bottom:24px;right:24px;z-index:1000;display:flex;flex-direction:column;gap:12px}
.toast{display:flex;align-items:center;gap:12px;padding:14px 20px;background:var(--bg-secondary);border:1px solid var(--border);border-radius:var(--radius);box-shadow:var(--shadow-lg);animation:slideIn 0.3s ease;min-width:300px}
.toast.success{border-left:3px solid var(--success)}
.toast.error{border-left:3px solid var(--error)}
.toast.warning{border-left:3px solid var(--warning)}
.toast-icon{width:20px;height:20px;flex-shrink:0}
.toast.success .toast-icon{color:var(--success)}
.toast.error .toast-icon{color:var(--error)}
.toast.warning .toast-icon{color:var(--warning)}
.toast-message{font-size:14px;color:var(--text-primary)}
@keyframes slideIn{from{transform:translateX(100%);opacity:0}to{transform:translateX(0);opacity:1}}
.empty-state{text-align:center;padding:60px 20px;color:var(--text-muted)}
.empty-state svg{width:64px;height:64px;margin-bottom:16px;opacity:0.5}
.empty-state h3{font-size:18px;color:var(--text-secondary);margin-bottom:8px}
.empty-state p{font-size:14px;max-width:400px;margin:0 auto}
.loading{display:flex;align-items:center;justify-content:center;padding:40px}
.spinner{width:32px;height:32px;border:3px solid var(--border);border-top-color:var(--accent);border-radius:50%;animation:spin 0.8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.hidden{display:none!important}
.csv-help-btn{display:inline-flex;align-items:center;justify-content:center;width:28px;height:28px;border-radius:50%;background:var(--bg-tertiary);color:var(--text-muted);border:1px solid var(--border);font-size:14px;font-weight:700;cursor:pointer;transition:var(--transition);margin-left:4px}
.csv-help-btn:hover{background:var(--accent-dim);color:var(--accent);border-color:var(--accent)}
.modal-overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.6);z-index:2000;display:flex;align-items:center;justify-content:center;animation:fadeIn 0.2s ease}
.modal{background:var(--bg-secondary);border:1px solid var(--border);border-radius:var(--radius-lg);padding:32px;max-width:520px;width:90%;max-height:80vh;overflow-y:auto;box-shadow:0 20px 60px rgba(0,0,0,0.4)}
.modal-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px}
.modal-title{font-size:18px;font-weight:600;color:var(--text-primary)}
.modal-close{display:flex;align-items:center;justify-content:center;width:32px;height:32px;border:none;background:var(--bg-tertiary);color:var(--text-muted);border-radius:var(--radius);cursor:pointer;font-size:18px;transition:var(--transition)}
.modal-close:hover{background:var(--bg-elevated);color:var(--text-primary)}
.modal-body p{font-size:14px;color:var(--text-secondary);margin-bottom:16px;line-height:1.6}
.modal-body pre{background:var(--bg-primary);border:1px solid var(--border);border-radius:var(--radius);padding:16px;font-family:'JetBrains Mono',monospace;font-size:13px;color:var(--text-primary);overflow-x:auto;margin-bottom:16px}
.modal-body code{font-family:'JetBrains Mono',monospace;font-size:13px;background:var(--bg-tertiary);padding:1px 5px;border-radius:3px;color:var(--accent)}
.modal-body ul{padding-left:20px;margin-bottom:12px}
.modal-body li{font-size:14px;color:var(--text-secondary);margin-bottom:6px}
@keyframes fadeIn{from{opacity:0}to{opacity:1}}
.action-btns{display:flex;gap:8px}
.action-btn{padding:6px 10px;border:none;background:var(--bg-tertiary);color:var(--text-secondary);border-radius:var(--radius-sm);cursor:pointer;transition:var(--transition);display:flex;align-items:center;gap:4px;font-size:12px}
.action-btn:hover{background:var(--accent);color:white}
.action-btn.danger:hover{background:var(--error)}
.footer{text-align:center;padding:24px;color:var(--text-muted);font-size:13px;border-top:1px solid var(--border)}
.footer a{color:var(--accent);text-decoration:none}
@media(max-width:768px){.header{padding:16px}.main{padding:16px}.tabs{width:100%;overflow-x:auto}.form-grid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="app">
  <header class="header">
    <div class="header-content">
      <div class="logo-section">
        <img src="lasu.png" alt="LASU Logo" class="logo" onerror="this.style.display=none">
        <div class="brand">
          <div class="brand-title">LASU Certificate Generator</div>
          <div class="brand-subtitle"><span>Lagos State University</span> - Faculty of Computing - Dept. of Computer Science</div>
        </div>
      </div>
      <div class="theme-toggle">
        <button class="theme-btn" data-theme="light" title="Light mode" onclick="setTheme('light')">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="3"/><line x1="12" y1="21" x2="12" y2="23"/><line x1="4.22" y1="4.22" x2="5.64" y2="5.64"/><line x1="18.36" y1="18.36" x2="19.78" y2="19.78"/><line x1="1" y1="12" x2="3" y2="12"/><line x1="21" y1="12" x2="23" y2="12"/><line x1="4.22" y1="19.78" x2="5.64" y2="18.36"/><line x1="18.36" y1="5.64" x2="19.78" y2="4.22"/></svg>
        </button>
        <button class="theme-btn" data-theme="dark" title="Dark mode" onclick="setTheme('dark')">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18"><path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"/></svg>
        </button>
        <button class="theme-btn" data-theme="system" title="Match system" onclick="setTheme('system')">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="18" height="18"><rect x="2" y="3" width="20" height="14" rx="2" ry="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg>
        </button>
      </div>
    </div>
  </header>
  <main class="main">
    <div class="tabs">
      <button class="tab active" data-tab="dashboard">Dashboard</button>
      <button class="tab" data-tab="students">Students</button>
      <button class="tab" data-tab="generate">Generate</button>
      <button class="tab" data-tab="stats">Statistics</button>
    </div>
    <div id="dashboard-tab" class="tab-content">
      <div class="stats-grid">
        <div class="stat-card"><div class="stat-label">Total Students</div><div class="stat-value" id="stat-total">0</div></div>
        <div class="stat-card"><div class="stat-label">Average GPA</div><div class="stat-value accent" id="stat-gpa">0.00</div></div>
        <div class="stat-card"><div class="stat-label">Certificates Generated</div><div class="stat-value success" id="stat-certs">0</div></div>
        <div class="stat-card"><div class="stat-label">Active Courses</div><div class="stat-value" id="stat-courses">0</div></div>
      </div>
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
          Quick Actions</h2>
        </div>
        <div class="btn-group">
          <button class="btn btn-primary" onclick="switchTab('students')">Add Student</button>
          <button class="btn btn-secondary" onclick="switchTab('generate')">Generate Certificates</button>
          <button class="btn btn-secondary" onclick="loadCSV()">Import CSV</button><button class="csv-help-btn" onclick="openCSVHelp()" title="CSV format help">?</button>
        </div>
      </div>
    </div>
    <div id="students-tab" class="tab-content hidden">
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><line x1="19" y1="8" x2="19" y2="14"/><line x1="22" y1="11" x2="16" y2="11"/></svg>
          Add Student</h2>
        </div>
        <div class="form-grid">
          <div class="form-group"><label>Student Name</label><input type="text" id="student-name" placeholder="e.g., John Adebayo"></div>
          <div class="form-group"><label>Course</label><input type="text" id="student-course" placeholder="e.g., CSC 226"></div>
          <div class="form-group"><label>Grade</label><input type="text" id="student-grade" placeholder="e.g., A, B+, 85"></div>
        </div>
        <div class="btn-group">
          <button class="btn btn-primary" onclick="addStudent()">Add Student</button>
          <button class="btn btn-secondary" onclick="loadCSV()">Import from CSV</button><button class="csv-help-btn" onclick="openCSVHelp()" title="CSV format help">?</button>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
          Search Students</h2>
        </div>
        <div class="form-grid">
          <div class="form-group"><label>Search Query</label><input type="text" id="search-query" placeholder="Search by name or course..." oninput="searchStudents()"></div>
          <div class="form-group"><label>Search By</label><select id="search-type"><option value="name">Name</option><option value="course">Course</option></select></div>
        </div>
      </div>
      <div class="card">
        <div class="card-header">
          <h2 class="card-title">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
            All Students
          </h2>
          <span id="student-count" style="color:var(--text-muted);font-size:14px">0 students</span>
        </div>
        <div class="table-container">
          <table><thead><tr><th>Name</th><th>Course</th><th>Grade</th><th>GPA</th><th>Actions</th></tr></thead>
          <tbody id="students-table"><tr><td colspan="5" class="empty-state"><h3>No students yet</h3><p>Add students manually or import from CSV.</p></td></tr></tbody>
          </table>
        </div>
      </div>
    </div>
    <div id="generate-tab" class="tab-content hidden">
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>
          Generate Certificate</h2>
        </div>
        <div class="form-grid">
          <div class="form-group"><label>Student Name</label><input type="text" id="gen-name" placeholder="Leave empty to generate all"><small style="color:var(--text-muted);font-size:12px">Leave empty to generate for all students</small></div>
          <div class="form-group"><label>Certificate Type</label><select id="gen-type"><option value="0">Certificate of Excellence</option><option value="1">Certificate of Completion</option><option value="2">Certificate of Participation</option></select></div>
          <div class="form-group"><label>Template Style</label><select id="gen-style"><option value="0">Formal</option><option value="1">Modern</option><option value="2">Minimalist</option></select></div>
        </div>
        <div class="btn-group">
          <button class="btn btn-success" onclick="generateCert()">Generate Certificate</button>
          <button class="btn btn-secondary" onclick="generateAllCerts()">Generate for All Students</button>
        </div>
      </div>
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"/><polyline points="13 2 13 9 20 9"/></svg>
          Generated Certificates</h2>
        </div>
        <div class="table-container">
          <table><thead><tr><th>File Name</th><th>Type</th><th>Actions</th></tr></thead>
          <tbody id="certs-table"><tr><td colspan="3" class="empty-state"><h3>No certificates generated</h3><p>Generated certificates will appear here.</p></td></tr></tbody>
          </table>
        </div>
      </div>
    </div>
    <div id="stats-tab" class="tab-content hidden">
      <div class="stats-grid">
        <div class="stat-card"><div class="stat-label">Total Students</div><div class="stat-value" id="stats-total">0</div></div>
        <div class="stat-card"><div class="stat-label">Average GPA</div><div class="stat-value accent" id="stats-gpa">0.00</div></div>
      </div>
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="18" y1="20" x2="18" y2="10"/><line x1="12" y1="20" x2="12" y2="4"/><line x1="6" y1="20" x2="6" y2="14"/></svg>
          Course Distribution</h2>
        </div>
        <div id="course-stats" class="empty-state"><p>No data available</p></div>
      </div>
      <div class="card">
        <div class="card-header"><h2 class="card-title">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 20V10"/><path d="M18 20V4"/><path d="M6 20v-4"/></svg>
          Grade Distribution</h2>
        </div>
        <div id="grade-stats" class="empty-state"><p>No data available</p></div>
      </div>
    </div>
  </main>
  <footer class="footer">
    <p>LASU Certificate Generator 2026 | Lagos State University, Faculty of Computing</p>
    <p style="margin-top:4px">Department of Computer Science - CSC 226 Group Project</p>
  </footer>
</div>
<div class="toast-container" id="toast-container"></div>
<input type="file" id="csv-file-input" accept=".csv" style="display:none">
<div id="csv-help-modal" class="modal-overlay hidden" onclick="if(event.target===this)closeCSVHelp()">
  <div class="modal">
    <div class="modal-header">
      <div class="modal-title">CSV File Format</div>
      <button class="modal-close" onclick="closeCSVHelp()">&times;</button>
    </div>
    <div class="modal-body">
      <p>Your CSV file should have three columns: <strong>Name</strong>, <strong>Course</strong>, and <strong>Grade</strong>. An optional header row is allowed but not required.</p>
      <p><strong>Accepted grade formats:</strong></p>
      <ul>
        <li>Letter grades: <code>A</code>, <code>A+</code>, <code>A-</code>, <code>B+</code>, <code>B</code>, <code>B-</code>, <code>C+</code>, <code>C</code>, <code>C-</code>, <code>D+</code>, <code>D</code>, <code>D-</code>, <code>F</code></li>
        <li>Numeric grades: <code>0</code> to <code>100</code></li>
      </ul>
      <p><strong>Example:</strong></p>
      <pre>Name,Course,Grade
Alice Johnson,CSC 226,A
Bob Smith,CSC 226,B+
Carol Williams,MATH 100,A-
David Brown,MATH 100,B
Eve Davis,CSC 226,A</pre>
      <p style="color:var(--text-muted);font-size:13px">To import, click <strong>Browse CSV</strong> and select a <code>.csv</code> file from your computer. You can also paste CSV content directly into the textarea below.</p>
    </div>
  </div>
</div>
<script>
const API = "";

function switchTab(t) {
  document.querySelectorAll(".tab").forEach(x => x.classList.remove("active"));
  document.querySelectorAll(".tab-content").forEach(x => x.classList.add("hidden"));
  document.querySelector("[data-tab=" + t + "]").classList.add("active");
  document.getElementById(t + "-tab").classList.remove("hidden");
  if (t === "dashboard") loadDashboard();
  if (t === "students") refreshTable();
  if (t === "stats") loadStats();
  if (t === "generate") loadCerts();
}

document.querySelectorAll(".tab").forEach(
  x => x.addEventListener("click", () => switchTab(x.dataset.tab))
);

function showMsg(m, t) {
  if (t === undefined) t = "success";
  const c = document.getElementById("toast-container");
  const toast = document.createElement("div");
  toast.className = "toast " + t;
  const icons = {
    success: '<svg class=toast-icon viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="20 6 9 17 4 12"/></svg>',
    error: '<svg class=toast-icon viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>',
    warning: '<svg class=toast-icon viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>'
  };
  toast.innerHTML = icons[t] + '<span class=toast-message>' + m + '</span>';
  c.appendChild(toast);
  setTimeout(() => {
    toast.style.animation = "slideIn 0.3s ease reverse";
    setTimeout(() => toast.remove(), 300);
  }, 3000);
}

function getGradeClass(g) {
  const x = g.charAt(0).toUpperCase();
  if (["A"].includes(x)) return "grade-a";
  if (["B"].includes(x)) return "grade-b";
  if (["C", "D"].includes(x)) return "grade-c";
  return "grade-f";
}

async function loadDashboard() {
  try {
    const r = await fetch(API + "/stats");
    const d = await r.json();
    if (d.status === "ok") {
      document.getElementById("stat-total").textContent = d.data.total || 0;
      document.getElementById("stat-gpa").textContent = (d.data.avgGpa || 0).toFixed(2);
      document.getElementById("stat-courses").textContent = Object.keys(d.data.courses || {}).length;
    }
    const cr = await fetch(API + "/certs");
    const cd = await cr.json();
    if (cd.status === "ok")
      document.getElementById("stat-certs").textContent = cd.files.length || 0;
  } catch (e) {
    console.error("Dashboard error:", e);
  }
}

async function refreshTable() {
  const tb = document.getElementById("students-table");
  tb.innerHTML = '<tr><td colspan=5 class=loading><div class=spinner></div></td></tr>';
  try {
    const r = await fetch(API + "/list");
    const d = await r.json();
    if (d.status === "ok" && d.data.length > 0) {
      document.getElementById("student-count").textContent =
        d.data.length + " student" + (d.data.length !== 1 ? "s" : "");
      tb.innerHTML = d.data.map(s =>
        '<tr>' +
          '<td><strong>' + s.name + '</strong></td>' +
          '<td>' + s.course + '</td>' +
          '<td><span class="grade-badge ' + getGradeClass(s.grade) + '">' + s.grade + '</span></td>' +
          '<td style="font-family:JetBrains Mono,monospace">' + s.gpa.toFixed(2) + '</td>' +
          '<td><div class=action-btns>' +
            '<button class="action-btn" onclick="quickGen(\'' + s.name.replace(/'/g, "\\'") + '\')">Generate</button>' +
            '<button class="action-btn danger" onclick="removeStudent(\'' + s.name.replace(/'/g, "\\'") + '\')">Remove</button>' +
          '</div></td>' +
        '</tr>'
      ).join("");
    } else {
      tb.innerHTML = '<tr><td colspan=5 class=empty-state><h3>No students yet</h3><p>Add students manually or import from CSV.</p></td></tr>';
    }
  } catch (e) {
    showMsg("Error loading students", "error");
  }
}

async function addStudent() {
  const n = document.getElementById("student-name").value.trim();
  const c = document.getElementById("student-course").value.trim();
  const g = document.getElementById("student-grade").value.trim();
  if (!n || !c || !g) { showMsg("Please fill all fields", "warning"); return; }
  try {
    const r = await fetch(API + "/add", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "name=" + encodeURIComponent(n) + "&course=" + encodeURIComponent(c) + "&grade=" + encodeURIComponent(g)
    });
    const d = await r.json();
    if (d.status === "ok") {
      showMsg("Added " + n, "success");
      document.getElementById("student-name").value = "";
      document.getElementById("student-course").value = "";
      document.getElementById("student-grade").value = "";
      refreshTable();
      loadDashboard();
    } else {
      showMsg(d.message || "Error adding student", "error");
    }
  } catch (e) {
    showMsg("Error adding student", "error");
  }
}

async function removeStudent(n) {
  if (!confirm("Remove " + n + "?")) return;
  try {
    const r = await fetch(API + "/remove", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "name=" + encodeURIComponent(n)
    });
    const d = await r.json();
    if (d.status === "ok") {
      showMsg("Removed " + n, "success");
      refreshTable();
      loadDashboard();
    } else {
      showMsg(d.message || "Error removing student", "error");
    }
  } catch (e) {
    showMsg("Error removing student", "error");
  }
}

async function searchStudents() {
  const q = document.getElementById("search-query").value.trim();
  const t = document.getElementById("search-type").value;
  if (!q) { refreshTable(); return; }
  const tb = document.getElementById("students-table");
  tb.innerHTML = '<tr><td colspan=5 class=loading><div class=spinner></div></td></tr>';
  try {
    const r = await fetch(API + "/search?q=" + encodeURIComponent(q) + "&type=" + t);
    const d = await r.json();
    if (d.status === "ok" && d.data.length > 0) {
      tb.innerHTML = d.data.map(s =>
        '<tr>' +
          '<td><strong>' + s.name + '</strong></td>' +
          '<td>' + s.course + '</td>' +
          '<td><span class="grade-badge ' + getGradeClass(s.grade) + '">' + s.grade + '</span></td>' +
          '<td style="font-family:JetBrains Mono,monospace">' + s.gpa.toFixed(2) + '</td>' +
          '<td><div class=action-btns>' +
            '<button class="action-btn" onclick="quickGen(\'' + s.name.replace(/'/g, "\\'") + '\')">Generate</button>' +
            '<button class="action-btn danger" onclick="removeStudent(\'' + s.name.replace(/'/g, "\\'") + '\')">Remove</button>' +
          '</div></td>' +
        '</tr>'
      ).join("");
    } else {
      tb.innerHTML = '<tr><td colspan=5 class=empty-state><h3>No results found</h3><p>Try a different search term.</p></td></tr>';
    }
  } catch (e) {
    showMsg("Search error", "error");
  }
}

async function generateCert() {
  const n = document.getElementById("gen-name").value.trim();
  const t = document.getElementById("gen-type").value;
  const s = document.getElementById("gen-style").value;
  const body = "certType=" + t + "&style=" + s + (n ? "&name=" + encodeURIComponent(n) : "");
  try {
    const r = await fetch(API + "/generate", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body
    });
    const d = await r.json();
    if (d.status === "ok") {
      showMsg("Certificate generated!", "success");
      loadCerts();
      loadDashboard();
    } else {
      showMsg(d.message || "Error generating", "error");
    }
  } catch (e) {
    showMsg("Error generating certificate", "error");
  }
}

async function generateAllCerts() {
  const t = document.getElementById("gen-type").value;
  const s = document.getElementById("gen-style").value;
  try {
    const r = await fetch(API + "/generate", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "certType=" + t + "&style=" + s
    });
    const d = await r.json();
    if (d.status === "ok") {
      showMsg("Certificates generated for all!", "success");
      loadCerts();
      loadDashboard();
    } else {
      showMsg(d.message || "Error", "error");
    }
  } catch (e) {
    showMsg("Error", "error");
  }
}

function quickGen(n) {
  document.getElementById("gen-name").value = n;
  switchTab("generate");
}

async function loadCerts() {
  const tb = document.getElementById("certs-table");
  try {
    const r = await fetch(API + "/certs");
    const d = await r.json();
    if (d.status === "ok" && d.files.length > 0) {
      tb.innerHTML = d.files.map(f =>
        '<tr>' +
          '<td style="font-family:JetBrains Mono,monospace;font-size:13px">' + f + '</td>' +
          '<td>' + (f.includes("Excellence") ? "Excellence" : f.includes("Completion") ? "Completion" : "Participation") + '</td>' +
          '<td><button class="action-btn" onclick="window.open(\'output/' + f + '\',\'_blank\')">View</button></td>' +
        '</tr>'
      ).join("");
    } else {
      tb.innerHTML = '<tr><td colspan=3 class=empty-state><h3>No certificates generated</h3><p>Generated certificates will appear here.</p></td></tr>';
    }
  } catch (e) {
    showMsg("Error loading certificates", "error");
  }
}

async function loadStats() {
  try {
    const r = await fetch(API + "/stats");
    const d = await r.json();
    if (d.status === "ok") {
      document.getElementById("stats-total").textContent = d.data.total || 0;
      document.getElementById("stats-gpa").textContent = (d.data.avgGpa || 0).toFixed(2);
      const courses = d.data.courses || {};
      const grades = d.data.grades || {};
      document.getElementById("course-stats").innerHTML =
        Object.keys(courses).length > 0
          ? Object.entries(courses).map(([c, n]) =>
              '<div style="display:flex;justify-content:space-between;padding:12px 0;border-bottom:1px solid var(--border)">' +
                '<span>' + c + '</span>' +
                '<span style="color:var(--accent)">' + n + ' student' + (n !== 1 ? "s" : "") + '</span>' +
              '</div>'
            ).join("")
          : '<p>No data</p>';
      document.getElementById("grade-stats").innerHTML =
        Object.keys(grades).length > 0
          ? Object.entries(grades).map(([g, n]) =>
              '<div style="display:flex;justify-content:space-between;padding:12px 0;border-bottom:1px solid var(--border)">' +
                '<span><span class="grade-badge ' + getGradeClass(g) + '">' + g + '</span></span>' +
                '<span style="color:var(--success)">' + n + '</span>' +
              '</div>'
            ).join("")
          : '<p>No data</p>';
    }
  } catch (e) {
    showMsg("Error loading statistics", "error");
  }
}

function loadCSV() {
  document.getElementById("csv-file-input").click();
}

document.getElementById("csv-file-input").addEventListener("change", function() {
  const file = this.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = function(e) {
    const content = e.target.result;
    fetch(API + "/loadcontent", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: "content=" + encodeURIComponent(content)
    })
      .then(r => r.json())
      .then(d => {
        if (d.status === "ok") {
          showMsg("Loaded from " + file.name, "success");
          refreshTable();
          loadDashboard();
        } else {
          showMsg(d.message || "Error loading CSV", "error");
        }
      })
      .catch(() => showMsg("Error loading CSV", "error"));
  };
  reader.readAsText(file);
  this.value = "";
});

function openCSVHelp() {
  document.getElementById("csv-help-modal").classList.remove("hidden");
}

function closeCSVHelp() {
  document.getElementById("csv-help-modal").classList.add("hidden");
}

function setTheme(t) {
  document.documentElement.dataset.theme = t;
  localStorage.setItem("theme", t);
  document.querySelectorAll(".theme-btn").forEach(
    b => b.classList.toggle("active", b.dataset.theme === t)
  );
}

function initTheme() {
  const s = localStorage.getItem("theme") || "system";
  const t = s === "system"
    ? (window.matchMedia("(prefers-color-scheme:dark)").matches ? "dark" : "light")
    : s;
  document.documentElement.dataset.theme = t;
  document.querySelectorAll(".theme-btn").forEach(
    b => b.classList.toggle("active", b.dataset.theme === s)
  );
}

window.matchMedia("(prefers-color-scheme:dark)").addEventListener("change", () => {
  const s = localStorage.getItem("theme") || "system";
  if (s === "system") {
    const t = window.matchMedia("(prefers-color-scheme:dark)").matches ? "dark" : "light";
    document.documentElement.dataset.theme = t;
  }
});

initTheme();
loadDashboard();
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
            if (request.size() + received > MAX_REQUEST_SIZE) { closesocket(client); return; }
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
        // GET /certs — list generated certificate files in output/
        else if (method == "GET" && path == "/certs") {
            std::string json = R"({"status":"ok","files":[)";
            bool first = true;
            if (std::filesystem::exists("output")) {
                for (const auto& entry : std::filesystem::directory_iterator("output")) {
                    if (entry.path().extension() == ".html") {
                        if (!first) json += ",";
                        json += "\"" + jsonEscape(entry.path().filename().string()) + "\"";
                        first = false;
                    }
                }
            }
            json += "]}";
            sendJSON(200, json);
        }
        // GET /listcsv — list CSV files in the working directory
        else if (method == "GET" && path == "/listcsv") {
            std::string json = R"({"status":"ok","files":[)";
            bool first = true;
            if (std::filesystem::exists(".")) {
                for (const auto& entry : std::filesystem::directory_iterator(".")) {
                    if (entry.path().extension() == ".csv") {
                        if (!first) json += ",";
                        json += "\"" + jsonEscape(entry.path().filename().string()) + "\"";
                        first = false;
                    }
                }
            }
            json += "]}";
            sendJSON(200, json);
        }
        // GET /cwd — return current working directory path
        else if (method == "GET" && path == "/cwd") {
            std::string cwd = std::filesystem::current_path().string();
            std::string json = R"({"status":"ok","path":")" + jsonEscape(cwd) + "\"}";
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
        // POST /loadcontent — load CSV from uploaded content (file picker or paste)
        else if (method == "POST" && path == "/loadcontent") {
            auto fields = parseForm(body);
            if (fields.count("content") && !fields["content"].empty()) {
                std::string csvPath = ".tmp_upload.csv";
                std::ofstream tmp(csvPath);
                tmp << fields["content"];
                tmp.close();
                try {
                    gen.loadFromCsv(csvPath);
                    std::remove(csvPath.c_str());
                    sendJSON(200, R"({"status":"ok","message":"CSV loaded from upload"})");
                } catch (const std::exception& e) {
                    std::remove(csvPath.c_str());
                    sendJSON(200, R"({"status":"error","message":")" + jsonEscape(e.what()) + "\"}");
                }
            } else {
                sendJSON(200, R"({"status":"error","message":"No CSV content provided"})");
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
        // GET /lasu.png — serve the LASU logo image
        else if (method == "GET" && path == "/lasu.png") {
            std::ifstream file("assets/lasu.png", std::ios::binary);
            if (file) {
                std::ostringstream buf;
                buf << file.rdbuf();
                sendResponse(200, "image/png", buf.str());
            } else {
                sendJSON(404, R"({"status":"error","message":"Logo not found"})");
            }
        }
        // GET /output/* — serve generated certificate files
        else if (method == "GET" && path.find("/output/") == 0) {
            std::string filepath = path.substr(1);
            std::ifstream file(filepath);
            if (file) {
                std::ostringstream buf;
                buf << file.rdbuf();
                sendResponse(200, "text/html", buf.str());
            } else {
                sendJSON(404, R"({"status":"error","message":"File not found"})");
            }
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
