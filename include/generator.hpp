#pragma once
#include <map>
#include <mutex>
#include <vector>
#include <string>
#include "person.hpp"
#include "certificate.hpp"

// CertificateGenerator — central data manager.
// Uses std::map (keyed by student name) for O(log n) lookups.
// Provides CRUD operations, search, sort, statistics, and batch generation.
class CertificateGenerator {
    // Map stores students keyed by lowercased name — enables O(log n) case-insensitive lookup
    std::map<std::string, Student> students_;
    mutable std::mutex mutex_; // guards students_ for thread safety

public:
    // ── CRUD ──
    void addStudent(const Student& s);
    bool removeStudent(const std::string& name);

    // Case-insensitive find by exact name
    Student* findStudent(const std::string& name);
    const Student* findStudent(const std::string& name) const;

    // ── Search ──
    std::vector<Student> searchByName(const std::string& partial) const;
    std::vector<Student> searchByCourse(const std::string& course) const;
    // Grade range is in GPA points (0.0–4.0)
    std::vector<Student> searchByGradeRange(double min, double max) const;

    // ── Utility ──
    std::vector<Student> getAllStudents() const;

    // ── Sort — uses std::sort with custom lambda comparators ──
    void sortByName();
    void sortByGrade(bool descending = true);
    void sortByCourse();

    // ── Input ──
    void loadFromCsv(const std::string& filename);
    Student inputSingleStudent();

    // ── Certificate Generation (delegates to Certificate + Template) ──
    void generateCertificateFor(const std::string& name, CertType type, TemplateStyle style) const;
    void generateAllCertificates(CertType type, TemplateStyle style) const;

    // ── Statistics ──
    struct Statistics {
        int total;
        double avgGradePoints;
        std::map<std::string, int> courseCounts;
        std::map<std::string, int> gradeDistribution;
    };

    Statistics computeStats() const;
    void printStats() const;

    // ── Status ──
    size_t count() const { return students_.size(); }
    bool empty() const { return students_.empty(); }
};
