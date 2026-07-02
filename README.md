# LASU Certificate Generator v2.0

**Lagos State University — Department of Computer Science — CSC 226 Group 6**

A production-grade C++ web application for generating, managing, and printing student certificates with multiple templates, real-time statistics, and a fully interactive browser-based UI — no frameworks, no dependencies.

| | |
|---|---|---|
| **Course** | CSC 226 — Object-Oriented Programming I |
| **Group** | 6 — 20 Members |
| **Tech** | C++17, Winsock2 (custom HTTP server), Embedded HTML/CSS/JS |
| **Patterns** | Strategy, Template Method, Factory, Inheritance, Polymorphism, RAII |
| **Build** | `g++ -std=c++17 -I include -o certificate_generator.exe main.cpp src/*.cpp -lws2_32`   |
| **Port** | `localhost:8080` |
| **Usage Guide** | [lasu-csc226-group6.vercel.app](https://lasu-csc226-group6.vercel.app) |

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
| **CSV Import** | Load from CSV file using the file picker |
| **CSV File Browser** | Lists all `.csv` files in working directory (`GET /listcsv`) |
| **Manual Entry** | Add individual students with input validation |

### Web UI Tabs
| Tab | Purpose |
|---|---|
| **Dashboard** | Quick stats + action buttons |
| **Students** | Full table with search, sort, delete |
| **Generate** | Pick type + style, generate for one or all |
| **Statistics** | Grade distribution, per-course breakdown |

---

## Design Patterns

| Pattern | Implementation | File |
|---|---|---|
| **Strategy** | `CertificateTemplate` interface with 3 visual styles swapped at runtime | `include/certificate.hpp:17-49` |
| **Template Method** | `Certificate::generateHTML()` defines the algorithm skeleton | `include/certificate.hpp:59-73` |
| **Factory** | `makeTemplate()` and `makeCertificate()` create correct subclass from enum | `include/certificate.hpp:52,100` |
| **Inheritance** | `Person` (abstract) → `Student` (concrete) — `is-a` relationship | `include/person.hpp` |
| **Polymorphism** | 3 abstract base classes, 7 pure virtual methods, runtime vtable dispatch | Throughout |
| **Composition** | `Certificate` has-a `CertificateTemplate`; `HttpServer` has-a `CertificateGenerator` | `certificate.hpp:62` |
| **Encapsulation** | All private members, public getters/setters, validated mutators | Throughout |
| **Exception Handling** | Custom `ValidationException`, `FileException` | `include/exceptions.hpp` |
| **RAII** | `std::unique_ptr`, `std::lock_guard`, `std::ofstream` — zero manual cleanup | Throughout |

---

## Architecture

```
main.cpp
   |
   v
HttpServer — Custom Winsock2 HTTP server (localhost:8080)
   |           thread-per-connection, 12 REST endpoints
   v
CertificateGenerator — Central data manager (std::map CRUD)
   |                    O(log n) lookups, thread-safe with std::mutex
   v
Certificate ← CertificateTemplate — Template Method + Strategy
   |                               3 visual styles, 3 certificate types
   v
Person → Student — Inheritance + Polymorphism
                  Grade-to-GPA conversion, input validation
```

### Data Flow
```
Browser → HTTP (JSON fetch) → HttpServer → CertificateGenerator → Certificate + Template → HTML file
```

---

## File Structure

```
.
+-- main.cpp                        # Entry point — starts the server
+-- CMakeLists.txt                  # Build configuration
+-- students.csv                    # Sample student data (10 records)
+-- assets/lasu.png                  # LASU University logo
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
+-- output/                         # Generated certificate files
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
1. Go to the **Students** tab
2. Click **Import from CSV** → **Browse CSV** and select `students.csv`
3. Switch to **Dashboard** to see stats

### Generate Certificates
1. Go to the **Generate** tab
2. Pick a certificate type (Excellence, Completion, Participation)
3. Pick a template style (Formal, Modern, Minimalist)
4. Leave name blank for all students, or enter one name
5. Click **Generate**
6. Generated files appear in the certificates list — click to view

---

## API Reference

| Method | Path | Description | Response |
|---|---|---|---|
| GET | `/` | Web UI | HTML |
| GET | `/list` | All students | `{"status":"ok","data":[...]}` |
| GET | `/search?q=&type=name\|course` | Search students | `{"status":"ok","data":[...]}` |
| GET | `/stats` | Statistics | `{"status":"ok","data":{...}}` |
| GET | `/certs` | Generated certificate files | `{"status":"ok","files":[...]}` |
| GET | `/listcsv` | CSV files in directory | `{"status":"ok","files":[...]}` |
| GET | `/cwd` | Current working directory | `{"status":"ok","path":"..."}` |
| POST | `/add` | Add student | `{"status":"ok"}` |
| POST | `/remove` | Remove student | `{"status":"ok"}` |
| POST | `/load` | Load CSV from file | `{"status":"ok"}` |
| POST | `/loadcontent` | Load CSV from pasted text | `{"status":"ok"}` |
| POST | `/generate` | Generate certificates | `{"status":"ok"}` |
| POST | `/sort` | Sort student list | `{"status":"ok"}` |

---

## Screenshots

Screenshots are in the `assets/` folder and embedded in the usage guide at [lasu-csc226-group6.vercel.app](https://lasu-csc226-group6.vercel.app).

| File | Description |
|---|---|
| `assets/dashboard.png` | Dashboard tab with stats cards |
| `assets/students.png` | Student table with search |
| `assets/generate.png` | Certificate generation form |
| `assets/stats.png` | Statistics with grade distribution |
| `assets/cert-formal.png` | Formal certificate preview |
| `assets/cert-modern.png` | Modern certificate preview |
| `assets/cert-minimal.png` | Minimalist certificate preview |
| `assets/csv-import.png` | CSV import dialog |

---

## Group 6 — Members

| # | Name | Matric No. |
|---|---|---|
| 1 | Idowu Fawaz Olawunmi | 240591101 |
| 2 | Idowu Matthew | 240591102 |
| 3 | Ikechukwu Queen Chisom | 240591103 |
| 4 | Inedu Esther Ogaicha | 240591104 |
| 5 | Iniotuh Greatest Akanimoh | 240591105 |
| 6 | Iwakun Oluwasegun | 240591106 |
| 7 | Iwuno Vincent ChukwuEbuka | 240591107 |
| 8 | Iyaniwura Olabamiji George | 240591108 |
| 9 | Jawando Fuad Olamide | 240591109 |
| 10 | Jeremiah David Preye | 240591110 |
| 11 | Joseph Elizabeth Nifemi | 240591111 |
| 12 | Kalejaiye Halimah Temilade | 240591112 |
| 13 | Kasali Damilola | 240591113 |
| 14 | Kassim Oluwatimilehin A. | 240591114 |
| 15 | Kehinde Oyindamola Ayomide | 240591115 |
| 16 | Kolawole Abubakar Olaoluwa | 240591116 |
| 17 | Kwegan Sean Oluwatomilade | 240591117 |
| 18 | Lamina Rihanat Opemipo | 240591118 |
| 19 | Lawal Sahal Adeshayo | 240591119 |
| 20 | Lawal Ibrahim Oluwafemi | 240591120 |

---

## FAQ

**Q: Can I use CSV files from another directory?**
A: Yes. Enter the full path, e.g. `C:\Users\Name\Desktop\students.csv`.

**Q: Can I paste CSV data without a file?**
A: Yes. Click **Import from CSV** and select a `.csv` file using the file picker.

**Q: How do I view generated certificates?**
A: Go to the **Generate** tab — generated files are listed and clickable.

**Q: Can I print certificates?**
A: Yes. Open the `.html` file in your browser and press Ctrl+P.

**Q: What grade formats are accepted?**
A: Letter grades (A, A+, A-, B+, B, B-, etc.) and numeric grades (0-100).

**Q: How do I stop the server?**
A: Press Ctrl+C in the terminal where the server is running.

---

## Version History

| Version | Date | Changes |
|---|---|---|
| **2.0** | June 2026 | LASU branding, CSV paste, certificates tab, static file serving, enhanced UI |
| **1.0** | May 2026 | Initial release with core functionality |

---

*&copy; 2026 Lagos State University, Department of Computer Science. All rights reserved.*
