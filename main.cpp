/*
 * Certificate Generator v2.0
 * CSC 226 — Group 6
 *
 * Authors:
 *   1. Idowu Fawaz Olawunmi       240591101
 *   2. Idowu Matthew              240591102
 *   3. Ikechukwu Queen Chisom     240591103
 *   4. Inedu Esther Ogaicha       240591104
 *   5. Iniotuh Greatest Akanimoh  240591105
 *   6. Iwakun Oluwasegun          240591106
 *   7. Iwuno Vincent ChukwuEbuka  240591107
 *   8. Iyaniwura Olabamiji George 240591108
 *   9. Jawando Fuad Olamide       240591109
 *  10. Jeremiah David Preye      240591110
 *  11. Joseph Elizabeth Nifemi   240591111
 *  12. Kalejaiye Halimah Temilade 240591112
 *  13. Kasali Damilola            240591113
 *  14. Kassim Oluwatimilehin A.   240591114
 *  15. Kehinde Oyindamola Ayomide 240591115
 *  16. Kolawole Abubakar Olaoluwa  240591116
 *  17. Kwegan Sean Oluwatomilade   240591117
 *  18. Lamina Rihanat Opemipo     240591118
 *  19. Lawal Sahal Adeshayo       240591119
 *  20. Lawal Ibrahim Oluwafemi    240591120
 *
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

    std::cout << "  ========================================\n";
    std::cout << "   LASU Certificate Generator v2.0\n";
    std::cout << "   Lagos State University\n";
    std::cout << "   Department of Computer Science\n";
    std::cout << "   CSC 226 - Group 6\n";
    std::cout << "  ========================================\n\n";

    HttpServer server(gen, 8080);
    server.start();

    return 0;
}
