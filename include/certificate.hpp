#pragma once
#include <string>
#include <memory>
#include "person.hpp"

// Enums for user-facing choices — map to certificate types and visual styles
enum class CertType { Excellence, Completion, Participation };
enum class TemplateStyle { Formal, Modern, Minimalist };

std::string certTypeToString(CertType type);

// ── Strategy Pattern ─────────────────────────────────────────────────────
// CertificateTemplate is the Strategy interface. Each concrete subclass
// provides a different visual design for the certificate HTML output.
// The Certificate class holds a reference to a template, allowing the
// visual style to be swapped at runtime without changing the certificate logic.
class CertificateTemplate {
public:
    virtual ~CertificateTemplate() = default;

    // Three pure virtual methods define the template's interface
    virtual std::string header(const std::string& certTypeName) const = 0;
    virtual std::string styles() const = 0;
    virtual std::string body(const Student& student, const std::string& certTypeName) const = 0;
};

// FormalTemplate — traditional certificate with double border and serif font
class FormalTemplate : public CertificateTemplate {
public:
    std::string header(const std::string& certTypeName) const override;
    std::string styles() const override;
    std::string body(const Student& student, const std::string& certTypeName) const override;
};

// ModernTemplate — gradient background, rounded corners, sans-serif
class ModernTemplate : public CertificateTemplate {
public:
    std::string header(const std::string& certTypeName) const override;
    std::string styles() const override;
    std::string body(const Student& student, const std::string& certTypeName) const override;
};

// MinimalistTemplate — monospace font, clean lines, understated design
class MinimalistTemplate : public CertificateTemplate {
public:
    std::string header(const std::string& certTypeName) const override;
    std::string styles() const override;
    std::string body(const Student& student, const std::string& certTypeName) const override;
};

// Factory function — returns the right template subclass for a given style enum
std::unique_ptr<CertificateTemplate> makeTemplate(TemplateStyle style);

// ── Template Method Pattern ──────────────────────────────────────────────
// Certificate is the abstract base. generateHTML() defines the skeleton
// algorithm: header → styles → body. Subclasses only override getTypeName()
// and getDescriptor() to customise the content at specific steps.
// The actual visual design is delegated to the CertificateTemplate (Strategy).
class Certificate {
protected:
    const Student& student_;
    const CertificateTemplate& template_;
public:
    Certificate(const Student& student, const CertificateTemplate& tmpl);
    virtual ~Certificate() = default;

    // Template method — calls the three strategy methods in order
    virtual std::string generateHTML() const;

    // Pure virtual — each certificate type provides its own name and description
    virtual std::string getTypeName() const = 0;
    virtual std::string getDescriptor() const = 0;
};

// CertificateOfExcellence — for outstanding academic achievement
class CertificateOfExcellence : public Certificate {
public:
    using Certificate::Certificate;
    std::string getTypeName() const override;
    std::string getDescriptor() const override;
};

// CertificateOfCompletion — for successful course completion
class CertificateOfCompletion : public Certificate {
public:
    using Certificate::Certificate;
    std::string getTypeName() const override;
    std::string getDescriptor() const override;
};

// CertificateOfParticipation — for active participation
class CertificateOfParticipation : public Certificate {
public:
    using Certificate::Certificate;
    std::string getTypeName() const override;
    std::string getDescriptor() const override;
};

// Factory function — creates the correct Certificate subclass at runtime
std::unique_ptr<Certificate> makeCertificate(
    CertType type, const Student& student, const CertificateTemplate& tmpl);
