#include "generator.hpp"
#include "exceptions.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

// Case-insensitive string transform — used for search and lookup
static std::string toLower(std::string s) {
    for (auto& c : s) c = std::tolower(static_cast<unsigned char>(c));
    return s;
}

// ─── CRUD ────────────────────────────────────────────────────────────────────

void CertificateGenerator::addStudent(const Student& s) {
    if (s.getName().empty()) {
        throw ValidationException("Student name cannot be empty.");
    }
    if (s.getCourse().empty()) {
        throw ValidationException("Course name cannot be empty.");
    }
    if (!Student::isValidGrade(s.getGrade())) {
        throw ValidationException("Invalid grade: \"" + s.getGrade() + "\". Use letter grade (A, B+, etc.) or number 0-100.");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    students_[toLower(s.getName())] = s;
}

bool CertificateGenerator::removeStudent(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return students_.erase(toLower(name)) > 0;
}

Student* CertificateGenerator::findStudent(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = students_.find(toLower(name));
    return (it != students_.end()) ? &it->second : nullptr;
}

const Student* CertificateGenerator::findStudent(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = students_.find(toLower(name));
    return (it != students_.end()) ? &it->second : nullptr;
}

// ─── Search ──────────────────────────────────────────────────────────────────

// Finds all students whose name contains the given fragment (case-insensitive)
std::vector<Student> CertificateGenerator::searchByName(const std::string& partial) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string lowerPartial = toLower(partial);
    std::vector<Student> results;
    for (const auto& [name, student] : students_) {
        if (name.find(lowerPartial) != std::string::npos) {
            results.push_back(student);
        }
    }
    return results;
}

// Finds all students enrolled in a course matching the fragment
std::vector<Student> CertificateGenerator::searchByCourse(const std::string& course) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Student> results;
    for (const auto& [name, student] : students_) {
        if (student.getCourse().find(course) != std::string::npos) {
            results.push_back(student);
        }
    }
    return results;
}

// Filters students whose GPA points fall within [min, max]
std::vector<Student> CertificateGenerator::searchByGradeRange(double min, double max) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Student> results;
    for (const auto& [name, student] : students_) {
        double pts = student.gradeToPoints();
        if (pts >= min && pts <= max) {
            results.push_back(student);
        }
    }
    return results;
}

// ─── Utility ─────────────────────────────────────────────────────────────────

std::vector<Student> CertificateGenerator::getAllStudents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Student> results;
    results.reserve(students_.size());
    for (const auto& [name, student] : students_) {
        results.push_back(student);
    }
    return results;
}

// ─── Sort — uses std::sort with custom lambda comparators ────────────────────

void CertificateGenerator::sortByName() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Student*> ptrs;
    ptrs.reserve(students_.size());
    for (auto& [name, student] : students_) {
        ptrs.push_back(&student);
    }
    std::sort(ptrs.begin(), ptrs.end(),
              [](const Student* a, const Student* b) {
                  return toLower(a->getName()) < toLower(b->getName());
              });

    std::map<std::string, Student> sorted;
    for (Student* p : ptrs) {
        sorted[toLower(p->getName())] = *p;
    }
    students_ = std::move(sorted);
}

void CertificateGenerator::sortByGrade(bool descending) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Student*> ptrs;
    ptrs.reserve(students_.size());
    for (auto& [name, student] : students_) {
        ptrs.push_back(&student);
    }
    std::sort(ptrs.begin(), ptrs.end(),
              [descending](const Student* a, const Student* b) {
                  double gpaA = a->gradeToPoints();
                  double gpaB = b->gradeToPoints();
                  return descending ? (gpaA > gpaB) : (gpaA < gpaB);
              });

    std::map<std::string, Student> sorted;
    for (Student* p : ptrs) {
        sorted[toLower(p->getName())] = *p;
    }
    students_ = std::move(sorted);
}

void CertificateGenerator::sortByCourse() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Student*> ptrs;
    ptrs.reserve(students_.size());
    for (auto& [name, student] : students_) {
        ptrs.push_back(&student);
    }
    std::sort(ptrs.begin(), ptrs.end(),
              [](const Student* a, const Student* b) {
                  return a->getCourse() < b->getCourse();
              });

    std::map<std::string, Student> sorted;
    for (Student* p : ptrs) {
        sorted[toLower(p->getName())] = *p;
    }
    students_ = std::move(sorted);
}

// ─── Input ───────────────────────────────────────────────────────────────────

void CertificateGenerator::loadFromCsv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw FileException("Could not open file: " + filename);
    }

    std::string line;
    int lineNum = 0;
    int added = 0;

    // Read first line
    if (!std::getline(file, line)) return;
    lineNum++;

    // Check if first line is a header by trying to parse the third column as a grade
    auto tryParseFirstLine = [&](const std::string& ln) -> bool {
        std::stringstream ss(ln);
        std::string n, c, g;
        if (!std::getline(ss, n, ',') || !std::getline(ss, c, ',') || !std::getline(ss, g)) return false;
        auto trim = [](std::string& s) { s.erase(0, s.find_first_not_of(" \t\r\n")); s.erase(s.find_last_not_of(" \t\r\n") + 1); };
        trim(g);
        return Student::isValidGrade(g);
    };

    if (tryParseFirstLine(line)) {
        // First line is data — process it
        std::stringstream ss(line);
        std::string name, course, grade;
        if (std::getline(ss, name, ',') && std::getline(ss, course, ',') && std::getline(ss, grade)) {
            auto trim = [](std::string& s) { s.erase(0, s.find_first_not_of(" \t\r\n")); s.erase(s.find_last_not_of(" \t\r\n") + 1); };
            trim(name); trim(course); trim(grade);
            if (!name.empty()) {
                try { addStudent(Student(name, course, grade)); added++; }
                catch (const ValidationException& e) { std::cerr << "  Warning: Line " << lineNum << ": " << e.what() << " — skipping" << std::endl; }
            }
        }
    }

    while (std::getline(file, line)) {
        lineNum++;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string name, course, grade;

        if (!std::getline(ss, name, ',') ||
            !std::getline(ss, course, ',') ||
            !std::getline(ss, grade)) {
            std::cerr << "  Warning: Malformed CSV at line " << lineNum
                      << " — skipping" << std::endl;
            continue;
        }

        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") + 1);
        };
        trim(name); trim(course); trim(grade);

        if (name.empty()) {
            std::cerr << "  Warning: Empty name at line " << lineNum
                      << " — skipping" << std::endl;
            continue;
        }

        try {
            addStudent(Student(name, course, grade));
            added++;
        }
        catch (const ValidationException& e) {
            std::cerr << "  Warning: Line " << lineNum << ": " << e.what()
                      << " — skipping" << std::endl;
        }
    }

    std::cout << "  Loaded " << added << " student(s) from " << filename << std::endl;
}

Student CertificateGenerator::inputSingleStudent() {
    std::string name, course, grade;

    std::cout << "  Enter student name: ";
    std::getline(std::cin, name);

    std::cout << "  Enter course: ";
    std::getline(std::cin, course);

    std::cout << "  Enter grade (A, B+, 85, etc.): ";
    std::getline(std::cin, grade);

    return Student(name, course, grade);
}

// ─── Certificate Generation ──────────────────────────────────────────────────

void CertificateGenerator::generateCertificateFor(
    const std::string& name, CertType type, TemplateStyle style) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = students_.find(toLower(name));
    if (it == students_.end()) {
        std::cerr << "  Student \"" << name
                  << "\" not found." << std::endl;
        return;
    }

    const Student& s = it->second;
    auto tmpl = makeTemplate(style);
    auto cert = makeCertificate(type, s, std::move(tmpl));

    std::string safeName = s.getName();
    for (char& c : safeName) if (c == ' ') c = '_';

    std::string typeStr = certTypeToString(type);
    std::string styleStr = templateStyleToString(style);
    std::string filename = "output/" + safeName + "_" + typeStr + "_" + styleStr + ".html";
    std::filesystem::create_directories("output");
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "  Could not create file "
                  << filename << std::endl;
        return;
    }

    file << cert->generateHTML();
    file.close();
    std::cout << "  Generated: " << filename << std::endl;
}

void CertificateGenerator::generateAllCertificates(
    CertType type, TemplateStyle style) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (students_.empty()) {
        std::cout << "  No students in database." << std::endl;
        return;
    }

    std::filesystem::create_directories("output");

    for (const auto& [name, student] : students_) {
        auto tmpl = makeTemplate(style);
        auto cert = makeCertificate(type, student, std::move(tmpl));

        std::string safeName = student.getName();
        for (char& c : safeName) if (c == ' ') c = '_';

        std::string typeStr = certTypeToString(type);
        std::string styleStr = templateStyleToString(style);
        std::string filename = "output/" + safeName + "_" + typeStr + "_" + styleStr + ".html";
        std::ofstream file(filename);

        if (file.is_open()) {
            file << cert->generateHTML();
            file.close();
            std::cout << "  Generated: " << filename << std::endl;
        } else {
            std::cerr << "  Could not create file "
                      << filename << std::endl;
        }
    }
}

// ─── Statistics ───────────────────────────────────────────────────────────────

CertificateGenerator::Statistics CertificateGenerator::computeStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Statistics stats{};
    stats.total = static_cast<int>(students_.size());

    if (students_.empty()) return stats;

    double totalPoints = 0;
    for (const auto& [name, student] : students_) {
        totalPoints += student.gradeToPoints();
        stats.courseCounts[student.getCourse()]++;
        stats.gradeDistribution[student.getGrade()]++;
    }
    stats.avgGradePoints = totalPoints / students_.size();
    return stats;
}

void CertificateGenerator::printStats() const {
    // Reuse computeStats() instead of duplicating logic
    Statistics stats = computeStats();

    std::cout << "\n  ====== STATISTICS ======\n";
    std::cout << "  Total Students: " << stats.total << "\n";
    std::cout << "  Average GPA:    " << std::fixed
              << std::setprecision(2) << stats.avgGradePoints << "\n";

    std::cout << "\n  --- Per Course ---\n";
    for (const auto& [course, count] : stats.courseCounts) {
        std::cout << "    " << course << ": "
                  << count << " student(s)\n";
    }

    std::cout << "\n  --- Grade Distribution ---\n";
    for (const auto& [grade, count] : stats.gradeDistribution) {
        std::cout << "    " << grade << ": " << count << "\n";
    }
    std::cout << "  ========================\n";
}
