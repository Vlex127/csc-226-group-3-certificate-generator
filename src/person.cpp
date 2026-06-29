#include "person.hpp"
#include "exceptions.hpp"
#include <cctype>
#include <vector>

// ── Person ──

Person::Person(const std::string& name) : name_(name) {}

std::string Person::getName() const { return name_; }
void Person::setName(const std::string& name) { name_ = name; }

// ── Student ──

Student::Student(const std::string& name,
                 const std::string& course,
                 const std::string& grade)
    : Person(name), course_(course), grade_(grade) {}

std::string Student::getCourse() const { return course_; }
std::string Student::getGrade() const { return grade_; }

void Student::setCourse(const std::string& course) { course_ = course; }
void Student::setGrade(const std::string& grade) { grade_ = grade; }

// Validates grade as either a letter (A, B+, etc.) or a number 0–100
bool Student::isValidGrade(const std::string& grade) {
    if (grade.empty()) return false;

    static const std::vector<std::string> validLetters = {
        "A+", "A", "A-", "B+", "B", "B-", "C+", "C", "C-",
        "D+", "D", "D-", "F"
    };
    for (const auto& v : validLetters) {
        if (grade == v) return true;
    }

    // Check numeric grade (0–100)
    try {
        double val = std::stod(grade);
        return val >= 0.0 && val <= 100.0;
    } catch (...) {
        return false;
    }
}

// Converts letter/numeric grade to 4.0 GPA scale for stats and sorting
double Student::gradeToPoints() const {
    if (grade_ == "A+" || grade_ == "A")  return 4.0;
    if (grade_ == "A-") return 3.7;
    if (grade_ == "B+") return 3.3;
    if (grade_ == "B")  return 3.0;
    if (grade_ == "B-") return 2.7;
    if (grade_ == "C+") return 2.3;
    if (grade_ == "C")  return 2.0;
    if (grade_ == "C-") return 1.7;
    if (grade_ == "D+") return 1.3;
    if (grade_ == "D")  return 1.0;
    if (grade_ == "D-") return 0.7;
    if (grade_ == "F")  return 0.0;

    // If not a letter grade, parse as number (e.g. "85") and scale to GPA
    try {
        double num = std::stod(grade_);
        return num / 25.0;
    } catch (...) {
        return 0.0;
    }
}

// Implements pure virtual from Person — returns formatted summary
std::string Student::getDescription() const {
    return "Student: " + name_ + " | " + course_ + " | Grade: " + grade_;
}
