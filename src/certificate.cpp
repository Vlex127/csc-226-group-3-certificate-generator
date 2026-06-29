#include "certificate.hpp"
#include "exceptions.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

// Converts the CertType enum to a human-readable string
std::string certTypeToString(CertType type) {
    switch (type) {
        case CertType::Excellence:  return "Excellence";
        case CertType::Completion:  return "Completion";
        case CertType::Participation: return "Participation";
    }
    return "Unknown";
}

// ── Factory: Template ──
// Returns the correct CertificateTemplate subclass based on the enum
std::unique_ptr<CertificateTemplate> makeTemplate(TemplateStyle style) {
    switch (style) {
        case TemplateStyle::Formal:     return std::make_unique<FormalTemplate>();
        case TemplateStyle::Modern:     return std::make_unique<ModernTemplate>();
        case TemplateStyle::Minimalist: return std::make_unique<MinimalistTemplate>();
    }
    return std::make_unique<FormalTemplate>();
}

// ── Formal Template ──────────────────────────────────────────────────────

std::string FormalTemplate::header(const std::string& certTypeName) const {
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang='en'>\n<head>\n"
      << "<meta charset='UTF-8'>\n"
      << "<title>Certificate of " << certTypeName << "</title>\n"
      << "</head>\n<body>\n";
    return h.str();
}

std::string FormalTemplate::styles() const {
    // Traditional double-border certificate with serif font
    return R"(
<style>
  body {
    font-family: 'Georgia', serif;
    background: #f5f5f5;
    display: flex;
    justify-content: center;
    align-items: center;
    height: 100vh;
    margin: 0;
  }
  .certificate {
    border: 10px double #1a365d;
    background: white;
    padding: 50px;
    width: 700px;
    text-align: center;
    box-shadow: 0 0 20px rgba(0,0,0,0.1);
  }
  .header { font-size: 42px; color: #1a365d; margin-bottom: 20px; font-weight: bold; text-transform: uppercase; }
  .sub { font-size: 18px; color: #555; font-style: italic; margin-bottom: 40px; }
  .name { font-size: 32px; color: #2c5282; border-bottom: 2px solid #2c5282; display: inline-block; padding: 0 20px; margin-bottom: 20px; }
  .reason { font-size: 20px; color: #333; line-height: 1.6; margin-bottom: 50px; }
  .footer { display: flex; justify-content: space-between; margin-top: 50px; font-size: 16px; color: #666; }
  .signature { border-top: 1px solid #aaa; width: 200px; padding-top: 5px; }
</style>)";
}

std::string FormalTemplate::body(const Student& student, const std::string& certTypeName) const {
    std::ostringstream b;
    b << "<div class='certificate'>\n"
      << "  <div class='header'>Certificate of " << certTypeName << "</div>\n"
      << "  <div class='sub'>This honor is proudly presented to</div>\n"
      << "  <div class='name'>" << student.getName() << "</div>\n"
      << "  <div class='reason'>for successfully completing the course <br><strong>"
      << student.getCourse() << "</strong><br> with a grade of <strong>"
      << student.getGrade() << "</strong>.</div>\n"
      << "  <div class='footer'>\n"
      << "    <div>Date: June 2026</div>\n"
      << "    <div class='signature'>Department Coordinator</div>\n"
      << "  </div>\n</div>\n";
    return b.str();
}

// ── Modern Template ──────────────────────────────────────────────────────

std::string ModernTemplate::header(const std::string& certTypeName) const {
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang='en'>\n<head>\n"
      << "<meta charset='UTF-8'>\n"
      << "<title>Certificate of " << certTypeName << "</title>\n"
      << "</head>\n<body>\n";
    return h.str();
}

std::string ModernTemplate::styles() const {
    // Gradient background, rounded corners, coloured accent bar at top
    return R"(
<style>
  body {
    font-family: 'Helvetica', Arial, sans-serif;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    display: flex;
    justify-content: center;
    align-items: center;
    height: 100vh;
    margin: 0;
  }
  .certificate {
    background: white;
    border-radius: 20px;
    padding: 60px;
    width: 680px;
    text-align: center;
    box-shadow: 0 20px 60px rgba(0,0,0,0.3);
    position: relative;
    overflow: hidden;
  }
  .certificate::before {
    content: '';
    position: absolute;
    top: 0; left: 0;
    width: 100%; height: 8px;
    background: linear-gradient(90deg, #667eea, #764ba2);
  }
  .header { font-size: 38px; color: #667eea; margin-bottom: 10px; font-weight: 800; letter-spacing: 2px; }
  .sub { font-size: 16px; color: #999; margin-bottom: 30px; text-transform: uppercase; letter-spacing: 3px; }
  .name { font-size: 36px; color: #333; margin-bottom: 15px; }
  .badge { font-size: 14px; background: #667eea; color: white; display: inline-block; padding: 6px 24px; border-radius: 20px; margin-bottom: 25px; text-transform: uppercase; letter-spacing: 2px; }
  .reason { font-size: 18px; color: #555; line-height: 1.8; margin-bottom: 40px; }
  .footer { display: flex; justify-content: space-between; font-size: 14px; color: #aaa; border-top: 2px solid #eee; padding-top: 20px; }
</style>)";
}

std::string ModernTemplate::body(const Student& student, const std::string& certTypeName) const {
    std::ostringstream b;
    b << "<div class='certificate'>\n"
      << "  <div class='header'>Certificate of " << certTypeName << "</div>\n"
      << "  <div class='sub'>Presented to</div>\n"
      << "  <div class='name'>" << student.getName() << "</div>\n"
      << "  <div class='badge'>" << student.getGrade() << "</div>\n"
      << "  <div class='reason'>For outstanding performance in<br><strong>"
      << student.getCourse() << "</strong></div>\n"
      << "  <div class='footer'>\n"
      << "    <div>June 2026</div>\n"
      << "    <div>Department Coordinator</div>\n"
      << "  </div>\n</div>\n";
    return b.str();
}

// ── Minimalist Template ──────────────────────────────────────────────────

std::string MinimalistTemplate::header(const std::string& certTypeName) const {
    std::ostringstream h;
    h << "<!DOCTYPE html>\n<html lang='en'>\n<head>\n"
      << "<meta charset='UTF-8'>\n"
      << "<title>Certificate of " << certTypeName << "</title>\n"
      << "</head>\n<body>\n";
    return h.str();
}

std::string MinimalistTemplate::styles() const {
    // Monospace font, thin border, left-aligned text — clean and simple
    return R"(
<style>
  body {
    font-family: 'Courier New', monospace;
    background: #fafafa;
    display: flex;
    justify-content: center;
    align-items: center;
    height: 100vh;
    margin: 0;
  }
  .certificate {
    border: 1px solid #ccc;
    background: white;
    padding: 80px 60px;
    width: 600px;
    text-align: left;
  }
  .header { font-size: 28px; color: #111; margin-bottom: 40px; font-weight: normal; letter-spacing: 4px; text-transform: uppercase; }
  .sub { font-size: 12px; color: #888; margin-bottom: 10px; text-transform: uppercase; }
  .name { font-size: 30px; color: #000; margin-bottom: 30px; font-weight: bold; }
  .reason { font-size: 16px; color: #444; line-height: 2; margin-bottom: 50px; }
  .footer { border-top: 1px solid #ddd; padding-top: 20px; font-size: 12px; color: #888; }
</style>)";
}

std::string MinimalistTemplate::body(const Student& student, const std::string& certTypeName) const {
    std::ostringstream b;
    b << "<div class='certificate'>\n"
      << "  <div class='header'>Certificate of " << certTypeName << "</div>\n"
      << "  <div class='sub'>Awarded to</div>\n"
      << "  <div class='name'>" << student.getName() << "</div>\n"
      << "  <div class='reason'>"
      << "Course: <strong>" << student.getCourse() << "</strong><br>\n"
      << "Grade:  <strong>" << student.getGrade() << "</strong>\n"
      << "  </div>\n"
      << "  <div class='footer'>Date: June 2026 &mdash; Department Coordinator</div>\n"
      << "</div>\n";
    return b.str();
}

// ── Certificate (Template Method) ────────────────────────────────────────

Certificate::Certificate(const Student& student, const CertificateTemplate& tmpl)
    : student_(student), template_(tmpl) {}

// Template method — defines the skeleton algorithm for certificate generation.
// Subclasses only override getTypeName() to plug in at the right step.
// The actual HTML rendering is delegated to the strategy object (template_).
std::string Certificate::generateHTML() const {
    std::string typeName = getTypeName();
    std::string html;
    html += template_.header(typeName);
    html += template_.styles();
    html += template_.body(student_, typeName);
    html += "</body>\n</html>";
    return html;
}

// ── Certificate Subclasses ───────────────────────────────────────────────

std::string CertificateOfExcellence::getTypeName() const { return "Excellence"; }
std::string CertificateOfExcellence::getDescriptor() const {
    return "for outstanding academic achievement";
}

std::string CertificateOfCompletion::getTypeName() const { return "Completion"; }
std::string CertificateOfCompletion::getDescriptor() const {
    return "for successful course completion";
}

std::string CertificateOfParticipation::getTypeName() const { return "Participation"; }
std::string CertificateOfParticipation::getDescriptor() const {
    return "for active participation";
}

// ── Factory: Certificate ──
// Creates the correct Certificate subclass at runtime based on the enum
std::unique_ptr<Certificate> makeCertificate(
    CertType type, const Student& student, const CertificateTemplate& tmpl)
{
    switch (type) {
        case CertType::Excellence:   return std::make_unique<CertificateOfExcellence>(student, tmpl);
        case CertType::Completion:   return std::make_unique<CertificateOfCompletion>(student, tmpl);
        case CertType::Participation: return std::make_unique<CertificateOfParticipation>(student, tmpl);
    }
    return std::make_unique<CertificateOfCompletion>(student, tmpl);
}
