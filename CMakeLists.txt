cmake_minimum_required(VERSION 3.14)
project(CertificateGenerator VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HEADERS
    include/exceptions.hpp
    include/person.hpp
    include/certificate.hpp
    include/generator.hpp
    include/http_server.hpp
)

set(SOURCES
    src/person.cpp
    src/certificate.cpp
    src/generator.cpp
    src/http_server.cpp
)

add_executable(certificate_generator main.cpp ${SOURCES} ${HEADERS})
target_include_directories(certificate_generator PRIVATE include)
target_link_libraries(certificate_generator ws2_32)


