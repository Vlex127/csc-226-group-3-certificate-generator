#include "person.hpp"
#include "exceptions.hpp"
#include <cctype>
#include <map>

// ─── Static Grade Table (avoids duplication between isValidGrade and gradeToPoints) ───
static const std::map<std::string, double> GRADE_POINTS = {
    {"A+", 4.0}, {"A", 4.0}, {"A-", 3.7},
    {"B+", 3.3}, {"B", 3.0}, {"B-", 2.7},
    {"C+", 2.3}, {"C", 2.0}, {"C-", 1.7},
    {"D+", 1.3}, {"D", 1.0}, {"D-", 0.7},
    {"F",  0.0}
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

// Validates grade as either a letter (A, B+, etc.) or a number 0-100
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

// Converts letter/numeric grade to 4.0 GPA scale for stats and sorting
double Student::gradeToPoints() const {
    // Look up in static map
    auto it = GRADE_POINTS.find(grade_);
    if (it != GRADE_POINTS.end()) return it->second;

    // If not a letter grade, parse as number (e.g. "85") and scale to GPA
    try {
        double num = std::stod(grade_);
        return num / 25.0;  // 100 -> 4.0 scale
    } catch (...) {
        return 0.0;
    }
}

// Implements pure virtual from Person — returns formatted summary
std::string Student::getDescription() const {
    return "Student: " + name_ + " | " + course_ + " | Grade: " + grade_;
}
