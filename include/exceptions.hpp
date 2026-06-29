#pragma once
#include <stdexcept>
#include <string>

// ValidationException — thrown when student name/course/grade fails validation
class ValidationException : public std::exception {
    std::string msg_;
public:
    explicit ValidationException(const std::string& msg) : msg_(msg) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

// FileException — thrown when a file cannot be opened (e.g. CSV missing)
class FileException : public std::exception {
    std::string msg_;
public:
    explicit FileException(const std::string& msg) : msg_(msg) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};
