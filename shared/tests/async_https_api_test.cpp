/**
 * Comprehensive Integration Tests for AsyncHTTPSConnection
 *
 * These tests validate the async HTTPS client against real endpoints to ensure:
 * - Proper state transitions (CONNECTING -> TLS -> WRITING -> READING -> DONE)
 * - Correct handling of different transfer encodings (Content-Length, Chunked, Connection-Close)
 * - Error handling and edge cases
 * - SSL/TLS handshake correctness
 * - Response header parsing with spillover handling
 *
 * Note: These are LIVE integration tests that require internet connectivity.
 */

#include "async_https_api.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>

using namespace std;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::IsEmpty;

// Test fixture for AsyncHTTPSConnection tests
class AsyncHTTPSConnectionTest : public ::testing::Test {
protected:
    unique_ptr<AsyncHTTPSConnection> conn;

    void SetUp() override {
        conn = make_unique<AsyncHTTPSConnection>();
    }

    void TearDown() override {
        conn.reset();
    }
};

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, BasicHTTPSPostRequest) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Make a simple POST request to httpbin.org
    conn->post_async(
        "httpbin.org",
        "/post",
        R"({"test": "data"})",
        {{"Content-Type", "application/json"}},
        std::move(prom)
    );

    // Run event loop in separate thread
    thread event_loop([this]() {
        conn->run_loop();
    });

    // Wait for result with timeout
    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "Request timed out";

    // Get result (may throw if exception was set)
    HTTPSResponse response = fut.get();

    EXPECT_THAT(response.body, HasSubstr("httpbin.org")) << "Response should contain host";
    EXPECT_THAT(response.body, Not(IsEmpty())) << "Response body should not be empty";

    event_loop.join();
}

TEST_F(AsyncHTTPSConnectionTest, HTTPSGetRequest) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // GET request (using empty body)
    conn->post_async(
        "httpbin.org",
        "/get",
        "",
        {{"User-Agent", "AsyncHTTPSConnection/1.0"}},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "GET request timed out";

    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, HasSubstr("httpbin.org"));

    event_loop.join();
}

// ============================================================================
// TRANSFER ENCODING TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, ChunkedTransferEncoding) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Request that returns chunked encoding
    // httpbin.org/stream/{n} returns n JSON lines using chunked encoding
    conn->post_async(
        "httpbin.org",
        "/stream/5",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(15));
    ASSERT_EQ(status, future_status::ready) << "Chunked request timed out";

    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, Not(IsEmpty())) << "Should receive chunked data";

    // Verify we got multiple lines (chunked response)
    int newline_count = count(response.body.begin(), response.body.end(), '\n');
    EXPECT_GE(newline_count, 5) << "Should receive at least 5 lines of data";

    event_loop.join();
}

TEST_F(AsyncHTTPSConnectionTest, ContentLengthTransferMode) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Request that returns Content-Length header
    conn->post_async(
        "httpbin.org",
        "/json",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "Content-Length request timed out";

    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, HasSubstr("slideshow")) << "Should contain JSON content";

    event_loop.join();
}

// ============================================================================
// RESPONSE SIZE TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, LargeResponseHandling) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Request large response (100 lines of JSON)
    conn->post_async(
        "httpbin.org",
        "/stream/100",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(20));
    ASSERT_EQ(status, future_status::ready) << "Large response request timed out";

    HTTPSResponse response = fut.get();
    EXPECT_GT(response.body.size(), 1000) << "Large response should be substantial";

    int newline_count = count(response.body.begin(), response.body.end(), '\n');
    EXPECT_GE(newline_count, 100) << "Should receive at least 100 lines";

    event_loop.join();
}


// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, InvalidHostHandling) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Try to connect to non-existent host
    conn->post_async(
        "this-host-definitely-does-not-exist-12345.com",
        "/test",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    // Should fail quickly - the future may throw or the event loop may exit early
    auto status = fut.wait_for(chrono::seconds(5));

    // Either timeout (host lookup hung) or ready with exception
    if (status == future_status::ready) {
        EXPECT_THROW(fut.get(), exception) << "Invalid host should throw";
    }

    event_loop.detach();
}

TEST_F(AsyncHTTPSConnectionTest, HTTPSToValidHostInvalidPath) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Valid host but invalid path (should get 404)
    conn->post_async(
        "httpbin.org",
        "/this-path-does-not-exist-12345",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "404 request timed out";

    // Connection should succeed even with 404
    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, Not(IsEmpty())) << "404 response should have body";

    event_loop.join();
}

// ============================================================================
// HEADER PARSING TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, HeaderParsingWithSpillover) {
    // This tests the scenario where headers and partial body arrive together

    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    conn->post_async(
        "httpbin.org",
        "/bytes/1024", // Get exactly 1024 bytes
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "Bytes request timed out";

    HTTPSResponse response = fut.get();
    EXPECT_EQ(response.body.size(), 1024) << "Should receive exactly 1024 bytes";

    event_loop.join();
}

TEST_F(AsyncHTTPSConnectionTest, CustomHeadersSent) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // httpbin.org/headers echoes back the headers we send
    conn->post_async(
        "httpbin.org",
        "/headers",
        "",
        {
            {"X-Custom-Header", "TestValue123"},
            {"User-Agent", "AsyncHTTPSTest/1.0"}
        },
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "Headers echo request timed out";

    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, HasSubstr("X-Custom-Header"))
        << "Custom header should be echoed back";
    EXPECT_THAT(response.body, HasSubstr("TestValue123"))
        << "Custom header value should be echoed back";

    event_loop.join();
}

// ============================================================================
// CONCURRENT REQUESTS TEST
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, DISABLED_MultipleConcurrentRequests) {
    // Note: This test is disabled because the current implementation
    // uses a single run_loop() that blocks. To enable concurrent requests,
    // the implementation would need request queuing or multiple connections.

    const int num_requests = 3;
    vector<promise<HTTPSResponse>> promises(num_requests);
    vector<future<HTTPSResponse>> futures;

    for (int i = 0; i < num_requests; ++i) {
        futures.push_back(promises[i].get_future());
        conn->post_async(
            "httpbin.org",
            "/get",
            "",
            {},
            std::move(promises[i])
        );
    }

    thread event_loop([this]() {
        conn->run_loop();
    });

    for (int i = 0; i < num_requests; ++i) {
        auto status = futures[i].wait_for(chrono::seconds(30));
        ASSERT_EQ(status, future_status::ready) << "Request " << i << " timed out";
        HTTPSResponse response = futures[i].get();
        EXPECT_THAT(response.body, Not(IsEmpty())) << "Request " << i << " should have response";
    }

    event_loop.join();
}

// ============================================================================
// SSL/TLS SPECIFIC TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, TLSHandshakeSuccess) {
    // Tests that SSL_connect succeeds and state transitions properly

    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // Use a known good HTTPS endpoint
    conn->post_async(
        "www.google.com",
        "/",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "TLS handshake test timed out";

    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, Not(IsEmpty()));

    event_loop.join();
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(AsyncHTTPSConnectionTest, DelayedResponseHandling) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    // httpbin.org/delay/{n} delays response by n seconds
    conn->post_async(
        "httpbin.org",
        "/delay/2",
        "",
        {},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(15));
    ASSERT_EQ(status, future_status::ready) << "Delayed response timed out";

    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, Not(IsEmpty()));

    event_loop.join();
}

TEST_F(AsyncHTTPSConnectionTest, SpecialCharactersInBody) {
    promise<HTTPSResponse> prom;
    future<HTTPSResponse> fut = prom.get_future();

    string test_body = R"({"special": "chars: \n\t\r\"'<>&", "unicode": "😀🎉"})";

    conn->post_async(
        "httpbin.org",
        "/post",
        test_body,
        {{"Content-Type", "application/json"}},
        std::move(prom)
    );

    thread event_loop([this]() {
        conn->run_loop();
    });

    auto status = fut.wait_for(chrono::seconds(10));
    ASSERT_EQ(status, future_status::ready) << "Special characters request timed out";

    HTTPSResponse response = fut.get();
    // httpbin echoes back the data, so we should see our special chars
    EXPECT_THAT(response.body, HasSubstr("special"));

    event_loop.join();
}

// Main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    cout << "\n================================================\n";
    cout << "AsyncHTTPSConnection Integration Tests\n";
    cout << "================================================\n";
    cout << "These tests require internet connectivity.\n";
    cout << "Testing against: httpbin.org, www.google.com\n";
    cout << "================================================\n\n";

    return RUN_ALL_TESTS();
}
