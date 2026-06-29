# Certificate Generator v2.0

Course project for **CSC 226** — generates printable HTML certificates for
students using configurable templates and certificate types.

**Group members:** [Your Names Here]

---

## Build

### Option A — CMake (recommended)

```bash
cmake -S . -B build
cmake --build build
```

### Option B — Direct g++ (no CMake required)

```bash
g++ -std=c++17 -I include -o certificate_generator.exe main.cpp src/*.cpp -lws2_32
```

Both produce `certificate_generator.exe`.

---

## Run

```bash
./certificate_generator
```

This starts a local web server on **http://localhost:8080** and opens it in
your default browser automatically.

### What you'll see

A web UI with six tabs:

| Tab | What it does |
|---|---|
| **Dashboard** | Quick stats — student count, average GPA, course count |
| **Students** | Table of all students with search, sort, and delete |
| **Add Student** | Add one student manually (name, course, grade) |
| **Load CSV** | Batch-import from a `.csv` file (format: `Name,Course,Grade`) |
| **Generate** | Pick certificate type + template style, generate for one or all |
| **Statistics** | Per-course breakdown, grade distribution, average GPA |

Certificates are saved as `.html` files in the same directory as the
executable. Double-click any `.html` file to view/print it.

---

## Features

### 3 Certificate Types (polymorphism)
| Type | Description |
|---|---|
| **Excellence** | For outstanding academic achievement |
| **Completion** | For successful course completion |
| **Participation** | For active participation |

### 3 Template Styles (Strategy pattern)
| Style | Look |
|---|---|
| **Formal** | Double-border, serif font, traditional certificate |
| **Modern** | Gradient background, rounded corners, sans-serif |
| **Minimalist** | Monospace, clean lines, understated |

### Data Management
- Manual entry with input validation
- CSV batch import with line-level error reporting
- Case-insensitive search (by name or course)
- Sort by name, grade (ascending/descending), or course
- Remove individual students
- Full statistics with grade distribution

---

## File Structure

```
.
├── CMakeLists.txt              # Build system
├── main.cpp                    # Entry point — starts web server
├── students.csv                # Sample data to try
├── test.cpp                    # Test suite (50 tests)
├── include/
│   ├── exceptions.hpp          # ValidationException, FileException
│   ├── person.hpp              # Person → Student (inheritance)
│   ├── certificate.hpp         # Certificate hierarchy + Strategy pattern
│   ├── generator.hpp           # CertificateGenerator (map, search, sort, stats)
│   └── http_server.hpp         # HTTP server declaration
└── src/
    ├── person.cpp
    ├── certificate.cpp
    ├── generator.cpp
    └── http_server.cpp         # Winsock server + embedded web UI
```

---

## Running Tests

```bash
g++ -std=c++17 -I include -o test_certificate.exe test.cpp src/*.cpp -lws2_32
./test_certificate.exe
```

All 50 tests should pass.

---

## OOP Concepts Demonstrated

- **Inheritance & Polymorphism** — `Person` → `Student`; `Certificate` → 3 subclasses
- **Abstract Base Classes** — Pure virtual `getTypeName()`, `getDescriptor()`
- **Strategy Pattern** — `CertificateTemplate` interface with 3 concrete styles
- **Factory Pattern** — `makeCertificate()`, `makeTemplate()` factory functions
- **Composition** — Certificate "has a" CertificateTemplate
- **Exception Handling** — Custom `ValidationException`, `FileException`
- **Encapsulation** — All data private, accessed through getters/setters
- **std::map** — Student database with O(log n) lookup
- **std::sort** — Custom comparators for multi-field sorting
- **std::unique_ptr** — Polymorphic ownership for templates and certificates
