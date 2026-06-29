/*
 * Certificate Generator v2.0
 * CSC 226 — Group 3
 *
 * Authors:  [Your Names Here]
 * Course:   CSC 226
 * Date:     30th June 2026
 *
 * Generates HTML certificates from student data.
 * Features: 3 certificate types, 3 template styles,
 * CSV batch import, search/sort/statistics,
 * web-based UI (auto-launches in browser).
 */

#include "http_server.hpp"
#include <iostream>

int main() {
    CertificateGenerator gen;

    std::cout << "Initializing certificate generator...\n";

    HttpServer server(gen, 8080);
    server.start();

    return 0;
}
