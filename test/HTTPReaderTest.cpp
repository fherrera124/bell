#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <sstream>

// Code under test
#include "bell/http/Client.h"
#include "bell/http/Reader.h"
#include "bell/utils/Utils.h"

TEST_CASE("bell::http::Reader tests", "[bell::http::Reader]") {
  // HTTP response parsing tests
  SECTION("Parses correct HTTP responses") {
    std::istringstream mockResponse(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Hello-world");

    bell::http::Reader reader(bell::http::Direction::Response, &mockResponse);
    REQUIRE_NOTHROW(reader.readHeaders());

    // Should properly parse the response
    REQUIRE(reader.getStatusCode() == 200);
    REQUIRE(reader.getContentLength() == 11);
    REQUIRE(reader.getHeader("Content-Type") == "text/html");

    REQUIRE_THROWS(
        reader.readHeaders());  // Should throw if headers are read again

    // Should throw on request-specific methods
    REQUIRE_THROWS(reader.getMethod());
    REQUIRE_THROWS(reader.getPath());

    // Make sure the reader did not consume any of the body
    std::string content;
    mockResponse >> content;
    REQUIRE(content == "Hello-world");
  }

  SECTION("Throws on malformed response") {
    std::istringstream mockResponse(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type text/html\r\n"  // Missing colon
        "Content-Length: 11\r\n"
        "\r\n"
        "Hello-world");
    bell::http::Reader reader(bell::http::Direction::Response, &mockResponse);
    REQUIRE_THROWS(reader.readHeaders());
  }

  SECTION("Parses various status codes") {
    std::istringstream mockResponse(
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Not Found Page");
    bell::http::Reader reader(bell::http::Direction::Response, &mockResponse);
    REQUIRE_NOTHROW(reader.readHeaders());
    REQUIRE(reader.getStatusCode() == 404);
  }

  SECTION("Parses multiple headers and case insensitivity") {
    std::istringstream mockResponse(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "CONTENT-LENGTH: 11\r\n"
        "X-Custom-Header: TestValue\r\n"
        "\r\n"
        "Hello-world");
    bell::http::Reader reader(bell::http::Direction::Response, &mockResponse);
    REQUIRE_NOTHROW(reader.readHeaders());
    REQUIRE(reader.getStatusCode() == 200);
    REQUIRE(reader.getContentLength() == 11);
    REQUIRE(reader.getHeader("content-type") ==
            "text/html");  // Case insensitive
    REQUIRE(reader.getHeader("x-custom-header") == "TestValue");
  }

  // HTTP request parsing tests
  // New Sections for HTTP Requests:
  SECTION("Parses correct HTTP requests") {
    std::istringstream mockRequest(
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: TestAgent\r\n"
        "\r\n");
    bell::http::Reader reader(bell::http::Direction::Request, &mockRequest);
    REQUIRE_NOTHROW(reader.readHeaders());
    REQUIRE(reader.getMethod() == bell::http::Method::GET);
    REQUIRE(reader.getPath() == "/index.html");
    REQUIRE(reader.getHeader("Host") == "example.com");
    REQUIRE(reader.getHeader("User-Agent") == "TestAgent");
    REQUIRE_THROWS(reader.getStatusCode());
  }

  SECTION("Handles missing Host header in HTTP/1.1 requests") {
    std::istringstream mockRequest(
        "GET /index.html HTTP/1.1\r\n"
        "User-Agent: TestAgent\r\n"
        "\r\n");
    bell::http::Reader reader(bell::http::Direction::Request, &mockRequest);
    REQUIRE_THROWS(
        reader.readHeaders());  // Missing Host should cause error in HTTP/1.1
  }

  SECTION("Throws on malformed request lines") {
    std::istringstream mockRequest(
        "GET /index.html HTTP\r\n"  // Missing version
        "Host: example.com\r\n"
        "\r\n");
    bell::http::Reader reader(bell::http::Direction::Request, &mockRequest);
    REQUIRE_THROWS(reader.readHeaders());
  }

  SECTION("Parses various HTTP methods") {
    std::istringstream mockRequest(
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 18\r\n"
        "\r\n"
        "{\"key\":\"value\"}");
    bell::http::Reader reader(bell::http::Direction::Request, &mockRequest);
    REQUIRE_NOTHROW(reader.readHeaders());
    REQUIRE(reader.getMethod() == bell::http::Method::POST);
    REQUIRE(reader.getPath() == "/submit");
    REQUIRE(reader.getHeader("Content-Type") == "application/json");
  }

  SECTION("Parses multiple headers in HTTP requests") {
    std::istringstream mockRequest(
        "PUT /resource/123 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Authorization: Bearer token\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
    bell::http::Reader reader(bell::http::Direction::Request, &mockRequest);
    REQUIRE_NOTHROW(reader.readHeaders());
    REQUIRE(reader.getMethod() == bell::http::Method::PUT);
    REQUIRE(reader.getHeader("Authorization") == "Bearer token");
  }
}
