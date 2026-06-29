#include "generator.hpp"
#include "exceptions.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

// Case-insensitive string transform — used for search and lookup
static std::string toLower(std::string s) {
    for (auto& c : s) c = std::tolower(static_cast<unsigned char>(c));
    return s;
}

// ── CRUD ─────────────────────────────────────────────────────────────────

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
    // Map keyed by original name for display — lookup uses case-insensitive search
    students_[s.getName()] = s;
}

// Case-insensitive remove — iterates map comparing lowered names
bool CertificateGenerator::removeStudent(const std::string& name) {
    std::string lower = toLower(name);
    auto it = std::find_if(students_.begin(), students_.end(),
        [&](const auto& pair) { return toLower(pair.first) == lower; });
    if (it == students_.end()) return false;
    students_.erase(it);
    return true;
}

// Case-insensitive find by exact name — linear scan since map key is original case
Student* CertificateGenerator::findStudent(const std::string& name) {
    std::string lower = toLower(name);
    auto it = std::find_if(students_.begin(), students_.end(),
        [&](const auto& pair) { return toLower(pair.first) == lower; });
    return (it != students_.end()) ? &it->second : nullptr;
}

const Student* CertificateGenerator::findStudent(const std::string& name) const {
    std::string lower = toLower(name);
    auto it = std::find_if(students_.begin(), students_.end(),
        [&](const auto& pair) { return toLower(pair.first) == lower; });
    return (it != students_.end()) ? &it->second : nullptr;
}

// ── Search ───────────────────────────────────────────────────────────────

// Finds all students whose name contains the given fragment (case-insensitive)
std::vector<Student> CertificateGenerator::searchByName(const std::string& partial) const {
    std::string lowerPartial = toLower(partial);
    std::vector<Student> results;
    for (const auto& [name, student] : students_) {
        if (toLower(name).find(lowerPartial) != std::string::npos) {
            results.push_back(student);
        }
    }
    return results;
}

// Finds all students enrolled in a course matching the fragment
std::vector<Student> CertificateGenerator::searchByCourse(const std::string& course) const {
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
    std::vector<Student> results;
    for (const auto& [name, student] : students_) {
        double pts = student.gradeToPoints();
        if (pts >= min && pts <= max) {
            results.push_back(student);
        }
    }
    return results;
}

// ── Utility ──────────────────────────────────────────────────────────────

std::vector<Student> CertificateGenerator::getAllStudents() const {
    std::vector<Student> results;
    results.reserve(students_.size());
    for (const auto& [name, student] : students_) {
        results.push_back(student);
    }
    return results;
}

// ── Sort — std::sort with custom lambdas ─────────────────────────────────

void CertificateGenerator::sortByName() {
    std::vector<Student> vec = getAllStudents();
    std::sort(vec.begin(), vec.end(),
        [](const Student& a, const Student& b) {
            return a.getName() < b.getName();
        });
    students_.clear();
    for (const auto& s : vec) {
        students_[s.getName()] = s;
    }
}

void CertificateGenerator::sortByGrade(bool descending) {
    std::vector<Student> vec = getAllStudents();
    std::sort(vec.begin(), vec.end(),
        [descending](const Student& a, const Student& b) {
            return descending
                ? a.gradeToPoints() > b.gradeToPoints()
                : a.gradeToPoints() < b.gradeToPoints();
        });
    students_.clear();
    for (const auto& s : vec) {
        students_[s.getName()] = s;
    }
}

// Sorts by course first, then by name within the same course
void CertificateGenerator::sortByCourse() {
    std::vector<Student> vec = getAllStudents();
    std::sort(vec.begin(), vec.end(),
        [](const Student& a, const Student& b) {
            if (a.getCourse() != b.getCourse())
                return a.getCourse() < b.getCourse();
            return a.getName() < b.getName();
        });
    students_.clear();
    for (const auto& s : vec) {
        students_[s.getName()] = s;
    }
}

// ── File Input ───────────────────────────────────────────────────────────

// Reads CSV with format: Name,Course,Grade per line.
// Invalid lines are skipped with a warning — other lines continue processing.
void CertificateGenerator::loadFromCsv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw FileException("Could not open file: " + filename);
    }

    std::string line;
    int lineNum = 0;
    int loaded = 0;

    while (std::getline(file, line)) {
        lineNum++;
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string name, course, grade;

        std::getline(ss, name, ',');
        std::getline(ss, course, ',');
        std::getline(ss, grade, ',');

        try {
            addStudent(Student(name, course, grade));
            loaded++;
        } catch (const ValidationException& e) {
            // Gracefully skip bad lines, report to stderr
            std::cerr << "  \xe2\x9a\xa0 Line " << lineNum
                      << " skipped: " << e.what() << std::endl;
        }
    }

    std::cout << "  Loaded " << loaded << " student(s) from "
              << filename << std::endl;
}

// ── Console Input ────────────────────────────────────────────────────────

Student CertificateGenerator::inputSingleStudent() {
    std::string name, course, grade;

    std::cout << "  Enter Full Name: ";
    std::getline(std::cin, name);

    std::cout << "  Enter Course Title: ";
    std::getline(std::cin, course);

    std::cout << "  Enter Grade (e.g., A, B+, 93): ";
    std::getline(std::cin, grade);

    Student s(name, course, grade);
    addStudent(s);
    return s;
}

// ── Certificate Generation ───────────────────────────────────────────────

// Generate a certificate for a specific student by name
void CertificateGenerator::generateCertificateFor(
    const std::string& name, CertType type, TemplateStyle style) const
{
    const Student* s = findStudent(name);
    if (!s) {
        std::cerr << "  \xe2\x9c\x97 Student \"" << name
                  << "\" not found." << std::endl;
        return;
    }

    // Factory methods create the correct polymorphic types
    auto tmpl = makeTemplate(style);
    auto cert = makeCertificate(type, *s, *tmpl);

    // Sanitise name for use as a filename
    std::string safeName = s->getName();
    for (char& c : safeName) if (c == ' ') c = '_';

    std::string typeStr = certTypeToString(type);
    std::string filename = safeName + "_" + typeStr + ".html";
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "  \xe2\x9c\x97 Could not create file "
                  << filename << std::endl;
        return;
    }

    file << cert->generateHTML();
    file.close();
    std::cout << "  \xe2\x9c\x93 Generated: " << filename << std::endl;
}

// Generate certificates for every student in the database
void CertificateGenerator::generateAllCertificates(
    CertType type, TemplateStyle style) const
{
    if (students_.empty()) {
        std::cout << "  No students in database." << std::endl;
        return;
    }

    auto tmpl = makeTemplate(style);

    for (const auto& [name, student] : students_) {
        auto cert = makeCertificate(type, student, *tmpl);

        std::string safeName = name;
        for (char& c : safeName) if (c == ' ') c = '_';

        std::string typeStr = certTypeToString(type);
        std::string filename = safeName + "_" + typeStr + ".html";
        std::ofstream file(filename);

        if (file.is_open()) {
            file << cert->generateHTML();
            file.close();
            std::cout << "  \xe2\x9c\x93 Generated: " << filename << std::endl;
        } else {
            std::cerr << "  \xe2\x9c\x97 Could not create file "
                      << filename << std::endl;
        }
    }
}

// ── Statistics ───────────────────────────────────────────────────────────

CertificateGenerator::Statistics CertificateGenerator::computeStats() const {
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
