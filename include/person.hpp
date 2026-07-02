#pragma once
#include <string>

// Person — abstract base class. Demonstrates:
//   - Inheritance (Student is-a Person)
//   - Pure virtual function (getDescription)
//   - Polymorphic destruction (virtual destructor)
class Person {
protected:
    std::string name_;
public:
    explicit Person(const std::string& name = "");

    virtual ~Person() = default;

    std::string getName() const;
    void setName(const std::string& name);

    // Pure virtual — every Person subclass must describe itself
    virtual std::string getDescription() const = 0;
};

// Student — concrete subclass of Person. Adds course/grade data
// and grade-to-GPA conversion logic.
class Student : public Person {
    std::string course_;
    std::string grade_;
public:
    Student(const std::string& name = "",
            const std::string& course = "",
            const std::string& grade = "");

    std::string getCourse() const;
    std::string getGrade() const;

    void setCourse(const std::string& course);
    void setGrade(const std::string& grade);

    // Converts letter grade (A, B, etc.) or numeric score (0–100) to 5.0 CGPA points
    double gradeToPoints() const;

    // Static — validates grade format before storing
    static bool isValidGrade(const std::string& grade);

    std::string getDescription() const override;
};
