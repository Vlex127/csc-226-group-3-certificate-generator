#include "person.hpp"
#include "exceptions.hpp"
#include <cctype>
#include <map>

// ─── LASU 5.0 CGPA Grading Scale ────────────────────────────────────────────
// A = 5  (70–100%)  First Class Honours
// B = 4  (60–69.9%) Second Class Upper
// C = 3  (50–59.9%) Second Class Lower
// D = 2  (45–49.9%) Third Class Honours
// E = 1  (40–44.9%) Pass
// F = 0  (0–39.9%)  Fail
// ─────────────────────────────────────────────────────────────────────────────
static const std::map<std::string, double> GRADE_POINTS = {
    {"A", 5.0},
    {"B", 4.0},
    {"C", 3.0},
    {"D", 2.0},
    {"E", 1.0},
    {"F", 0.0}
};

// ─── Person ─────────────────────────────────────────────────────────────────

Person::Person(const std::string& name) : name_(name) {}

std::string Person::getName() const { return name_; }
void Person::setName(const std::string& name) { name_ = name; }

// ─── Student ───────────────────────────────────────────────────────────────

Student::Student(const std::string& name,
                 const std::string& course,
                 const std::string& grade)
    : Person(name), course_(course), grade_(grade) {}

std::string Student::getCourse() const { return course_; }
std::string Student::getGrade() const { return grade_; }

void Student::setCourse(const std::string& course) { course_ = course; }
void Student::setGrade(const std::string& grade) { grade_ = grade; }

// Validates grade as a LASU letter grade (A–F) or a numeric score (0–100)
bool Student::isValidGrade(const std::string& grade) {
    if (grade.empty()) return false;

    // Check letter grade against static map
    if (GRADE_POINTS.count(grade) > 0) return true;

    // Check numeric grade (0-100)
    try {
        double val = std::stod(grade);
        return val >= 0.0 && val <= 100.0;
    } catch (...) {
        return false;
    }
}

// Converts LASU letter grade or numeric score (0–100) to 5.0 CGPA scale
double Student::gradeToPoints() const {
    // Look up in static map
    auto it = GRADE_POINTS.find(grade_);
    if (it != GRADE_POINTS.end()) return it->second;

    // Numeric score → 5.0 CGPA scale
    try {
        double score = std::stod(grade_);
        if      (score >= 70.0) return 5.0;  // A
        else if (score >= 60.0) return 4.0;  // B
        else if (score >= 50.0) return 3.0;  // C
        else if (score >= 45.0) return 2.0;  // D
        else if (score >= 40.0) return 1.0;  // E
        else                    return 0.0;  // F
    } catch (...) {
        return 0.0;
    }
}

// Implements pure virtual from Person — returns formatted summary
std::string Student::getDescription() const {
    return "Student: " + name_ + " | " + course_ + " | Grade: " + grade_;
}
