# LASU Certificate Generator v2.0

**Lagos State University — Department of Computer Science**

> A production-grade C++ web application for generating, managing, and printing
> student certificates with multiple templates, real-time statistics, and
> a fully interactive browser-based UI — no frameworks, no dependencies.

| | | |
|---|---|---|
| **Course** | CSC 226 — Object-Oriented Programming II | |
| **Group** | 3 | |
| **Tech** | C++17, Winsock2 (custom HTTP server), Embedded HTML/CSS/JS | |
| **Patterns** | Strategy, Template Method, Factory, Inheritance, Polymorphism | | 
| **Build** | `g++ -std=c++17 -I include main.cpp src/*.cpp -lws2_32` | |

---

## Features

### Certificate Management
| Feature | Detail |
|---|---|
| **3 Certificate Types** | Excellence, Completion, Participation |
| **3 Template Styles** | Formal (serif/double-border), Modern (gradient/rounded), Minimalist (monospace/clean) |
| **LASU Branding** | All certificates include Lagos State University & Department of Computer Science |
| **Batch Generation** | Generate certificates for all students at once |
| **Live Preview** | Certificates saved as `.html` — open in any browser to view/print |

### Student Data Management
| Feature | Detail |
|---|---|
| **Dashboard** | Real-time stats: total students, average GPA, course count |
| **Student Table** | Search, sort (name/grade/course), add/remove students |
| **CSV Import (File)** | Load from any path — current directory or external (`C:\path\to\file.csv`) |
| **CSV Import (Paste)** | Paste CSV data directly from any source |
| **CSV File Browser** | Auto-lists all `.csv` files in working directory for one-click loading |
| **Manual Entry** | Add individual students with input validation |

### Statistics
| Feature | Detail |
|---|---|
| Per-course student counts | |
| Grade distribution (A, B+, etc.) | |
| Average GPA calculation | |
| Real-time updates across all tabs | |

### Web UI
| Tab | Purpose |
|---|---|
| **Dashboard** | Quick stats + action buttons |
| **Students** | Full table with search, sort, delete |
| **Add Student** | Manual entry form |
| **CSV Import** | File path input, paste textarea, auto-listed `.csv` files |
| **Generate** | Pick type + style, generate for one or all |
| **Certificates** | Browse all generated `.html` files with clickable links |
| **Statistics** | Grade distribution, per-course breakdown |

---

## Architecture

```
main.cpp
   |
   v
HttpServer — Custom Winsock2 HTTP server (localhost:8080)
   |
   v
CertificateGenerator — Central data manager (std::map CRUD)
   |
   v
Certificate ← CertificateTemplate — Template Method + Strategy
   |
   v
Person → Student — Inheritance + Polymorphism
```

### Design Patterns

| Pattern | Implementation | File |
|---|---|---|
| **Strategy** | `CertificateTemplate` interface with 3 visual styles swapped at runtime | `certificate.hpp:17-49` |
| **Template Method** | `Certificate::generateHTML()` defines the algorithm skeleton | `certificate.hpp:59-73` |
| **Factory** | `makeTemplate()` and `makeCertificate()` create correct subclass from enum | `certificate.hpp:52,100` |
| **Inheritance** | `Person` → `Student` | `person.hpp` |
| **Composition** | `Certificate` has-a `CertificateTemplate` | `certificate.hpp:62` |
| **Encapsulation** | All private members, public getters/setters | Throughout |
| **Exception Handling** | Custom `ValidationException`, `FileException` | `exceptions.hpp` |
| **RAII** | `std::unique_ptr`, `std::ofstream` | Throughout |

### File Structure

```
.
+-- main.cpp                        # Entry point — starts the server
+-- CMakeLists.txt                  # Build configuration
+-- students.csv                    # Sample data
+-- include/
|   +-- exceptions.hpp              # Custom exception classes
|   +-- person.hpp                  # Person (abstract) -> Student
|   +-- certificate.hpp             # Certificate + Template hierarchy
|   +-- generator.hpp               # CertificateGenerator CRUD + stats
|   +-- http_server.hpp             # HTTP server declaration
+-- src/
|   +-- person.cpp                  # Student implementation
|   +-- certificate.cpp             # Templates + certificate types
|   +-- generator.cpp               # Data management + CSV parsing
|   +-- http_server.cpp             # HTTP server + embedded web UI
+-- *.html                          # Generated certificate files
```

---

## Quick Start

### Build

```bash
g++ -std=c++17 -I include -o certificate_generator.exe main.cpp src/*.cpp -lws2_32
```

### Run

```bash
./certificate_generator
```

The server starts on **http://localhost:8080** and opens in your browser automatically.

### Load Sample Data

1. Go to the **CSV Import** tab
2. Click the `students.csv` file shown in the file list, or type `students.csv` and click Load
3. Switch to **Dashboard** to see stats, or **Students** to browse the table

### Generate Certificates

1. Go to the **Generate** tab
2. Pick a certificate type (Excellence, Completion, Participation)
3. Pick a template style (Formal, Modern, Minimalist)
4. Leave name blank for all students, or enter one name
5. Click **Generate**
6. Switch to the **Certificates** tab to see and open the generated files

---

## API Reference

| Method | Path | Description | Response |
|---|---|---|---|---|
| GET | `/` | Web UI | HTML |
| GET | `/list` | All students | `{"status":"ok","data":[...]}` |
| GET | `/search?q=&type=name\|course` | Search students | `{"status":"ok","data":[...]}` |
| GET | `/stats` | Statistics | `{"status":"ok","data":{...}}` |
| GET | `/certs` | Generated certificate files in `output/` | `{"status":"ok","files":[...]}` |
| GET | `/listcsv` | CSV files in directory | `{"status":"ok","files":[...]}` |
| GET | `/cwd` | Current working directory | `{"status":"ok","path":"..."}` |
| GET | `/output/*.html` | Serve generated certificate files | HTML |
| GET | `/*.html` | Serve other static files | HTML |
| POST | `/add` | Add student | `{"status":"ok"}` |
| POST | `/remove` | Remove student | `{"status":"ok"}` |
| POST | `/load` | Load CSV from file | `{"status":"ok"}` |
| POST | `/loadcontent` | Load CSV from pasted text | `{"status":"ok"}` |
| POST | `/generate` | Generate certificates | `{"status":"ok"}` |
| POST | `/sort` | Sort student list | `{"status":"ok"}` |

---

## FAQ

**Q: Can I use CSV files from another directory?**
A: Yes. Enter the full path, e.g. `C:\Users\Name\Desktop\students.csv` or `../data/students.csv`.

**Q: Can I paste CSV data without a file?**
A: Yes. Use the **CSV Import** tab and paste your data directly into the textarea (Option 2).

**Q: How do I view generated certificates?**
A: Go to the **Certificates** tab and click any filename. It opens in a new browser tab.

**Q: Can I print certificates?**
A: Yes. Open the `.html` file in your browser and press Ctrl+P.

**Q: How do I add just one student?**
A: Use the **Add Student** tab and fill in the name, course, and grade fields.

**Q: What grade formats are accepted?**
A: Letter grades (A, A+, A-, B+, B, B-, etc.) and numeric grades (0-100).

**Q: How do I stop the server?**
A: Press Ctrl+C in the terminal where the server is running.

---

## Contact

For inquiries, assistance, and support, contact the development team:

| | |
|---|---|
| **Course** | CSC 226 — Object-Oriented Programming II |
| **Session** | 2025/2026 |
| **Last Updated** | June 2026 |

---

## Version History

| Version | Date | Changes |
|---|---|---|
| **2.0** | June 2026 | LASU branding, CSV paste, certificates tab, static file serving, enhanced UI |
| **1.0** | May 2026 | Initial release with core functionality |

---

*&copy; 2026 Lagos State University, Department of Computer Science. All rights reserved.*
