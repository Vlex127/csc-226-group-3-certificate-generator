#pragma once
#include "generator.hpp"

// HttpServer — minimal HTTP server using Winsock.
// Serves the embedded web UI on localhost and exposes REST-like API
// endpoints that delegate to CertificateGenerator.
// Each request spawns a detached thread (handleClient).
class HttpServer {
    CertificateGenerator& gen_; // reference to shared data manager
    int port_;
    bool verbose_; // when true, log every request to stderr
public:
    HttpServer(CertificateGenerator& gen, int port = 8080, bool verbose = false);
    // Blocks until the process is killed — accepts connections in a loop
    void start();
};
