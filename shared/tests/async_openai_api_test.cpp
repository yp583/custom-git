/**
 * Integration Tests for AsyncOpenAIAPI
 *
 * These tests validate the async OpenAI API client against the live OpenAI API:
 * - Embedding endpoint functionality
 * - Chat completion endpoint functionality
 * - Concurrent request handling
 *
 * Note: These are LIVE integration tests that require:
 * - Internet connectivity
 * - Valid OPENAI_API_KEY environment variable
 */

#include "async_openai_api.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <cstdlib>
#include <fstream>

using namespace std;
using json = nlohmann::json;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::IsEmpty;

// Helper function to load API key from environment or .env file
string load_api_key() {
    // First try environment variable
    const char* env_key = getenv("OPENAI_API_KEY");
    if (env_key && strlen(env_key) > 0) {
        return string(env_key);
    }

    // Fall back to .env file
    ifstream env_file(".env");
    if (!env_file.is_open()) {
        // Try parent directories
        env_file.open("../.env");
        if (!env_file.is_open()) {
            env_file.open("../../.env");
            if (!env_file.is_open()) {
                env_file.open("../../../.env");
            }
        }
    }

    if (env_file.is_open()) {
        string line;
        while (getline(env_file, line)) {
            if (line.substr(0, 14) == "OPENAI_API_KEY") {
                return line.substr(15); // Skip "OPENAI_API_KEY="
            }
        }
    }

    return "";
}

// Test fixture for AsyncOpenAIAPI tests
class AsyncOpenAIAPITest : public ::testing::Test {
protected:
    string api_key;

    void SetUp() override {
        api_key = load_api_key();
        if (api_key.empty()) {
            GTEST_SKIP() << "OPENAI_API_KEY not found. Set environment variable or create .env file.";
        }
    }

    void TearDown() override {
    }
};

// ============================================================================
// TEST 1: Embedding Endpoint
// ============================================================================

TEST_F(AsyncOpenAIAPITest, EmbeddingEndpointWorks) {
    AsyncHTTPSConnection conn;
    AsyncOpenAIAPI api(conn, api_key);

    // Queue an embedding request
    future<HTTPSResponse> fut = api.async_embedding(
        "Hello, world! This is a test of the embedding API."
    );

    // Run event loop in separate thread
    thread event_loop([&api]() {
        api.run_requests();
    });

    // Wait for result with timeout
    auto status = fut.wait_for(chrono::seconds(30));
    ASSERT_EQ(status, future_status::ready) << "Embedding request timed out";

    // Get result
    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, Not(IsEmpty())) << "Response body should not be empty";

    // Parse JSON and verify structure
    json response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("data")) << "Response should contain 'data' field";
    EXPECT_TRUE(response_json["data"].is_array()) << "'data' should be an array";
    EXPECT_GT(response_json["data"].size(), 0) << "'data' array should not be empty";
    EXPECT_TRUE(response_json["data"][0].contains("embedding")) << "First item should have 'embedding'";
    EXPECT_TRUE(response_json["data"][0]["embedding"].is_array()) << "'embedding' should be an array";
    EXPECT_GT(response_json["data"][0]["embedding"].size(), 0) << "Embedding vector should not be empty";

    event_loop.join();
}

// ============================================================================
// TEST 2: Chat Completion Endpoint
// ============================================================================

TEST_F(AsyncOpenAIAPITest, ChatEndpointWorks) {
    AsyncHTTPSConnection conn;
    AsyncOpenAIAPI api(conn, api_key);

    // Create chat messages
    json messages = {
        {
            {"role", "system"},
            {"content", "You are a helpful assistant. Respond briefly."}
        },
        {
            {"role", "user"},
            {"content", "What is 2 + 2? Answer with just the number."}
        }
    };

    // Queue a chat request
    future<HTTPSResponse> fut = api.async_chat(
        messages,
        50,  // max_tokens
        0.0  // temperature (deterministic)
    );

    // Run event loop in separate thread
    thread event_loop([&api]() {
        api.run_requests();
    });

    // Wait for result with timeout
    auto status = fut.wait_for(chrono::seconds(30));
    ASSERT_EQ(status, future_status::ready) << "Chat request timed out";

    // Get result
    HTTPSResponse response = fut.get();
    EXPECT_THAT(response.body, Not(IsEmpty())) << "Response body should not be empty";

    // Parse JSON and verify structure
    json response_json = json::parse(response.body);
    EXPECT_TRUE(response_json.contains("choices")) << "Response should contain 'choices' field";
    EXPECT_TRUE(response_json["choices"].is_array()) << "'choices' should be an array";
    EXPECT_GT(response_json["choices"].size(), 0) << "'choices' array should not be empty";
    EXPECT_TRUE(response_json["choices"][0].contains("message")) << "First choice should have 'message'";
    EXPECT_TRUE(response_json["choices"][0]["message"].contains("content")) << "Message should have 'content'";

    // The response should contain "4" somewhere
    string content = response_json["choices"][0]["message"]["content"];
    EXPECT_THAT(content, HasSubstr("4")) << "Chat response should contain the answer '4'";

    event_loop.join();
}

// ============================================================================
// TEST 3: Concurrent Requests
// ============================================================================

TEST_F(AsyncOpenAIAPITest, ConcurrentRequestsWork) {
    const int num_requests = 3;

    AsyncHTTPSConnection conn;
    AsyncOpenAIAPI api(conn, api_key);

    // Queue multiple embedding requests concurrently
    vector<string> test_texts = {
        "First test text for embedding",
        "Second test text for embedding",
        "Third test text for embedding"
    };

    vector<future<HTTPSResponse>> futures;
    for (int i = 0; i < num_requests; ++i) {
        futures.push_back(api.async_embedding(test_texts[i]));
    }

    // Run event loop in separate thread
    thread event_loop([&api]() {
        api.run_requests();
    });

    // Wait for all results with timeout
    vector<HTTPSResponse> responses;
    for (int i = 0; i < num_requests; ++i) {
        auto status = futures[i].wait_for(chrono::seconds(60));
        ASSERT_EQ(status, future_status::ready) << "Request " << i << " timed out";
        responses.push_back(futures[i].get());
    }

    // Verify all requests succeeded
    for (int i = 0; i < num_requests; ++i) {
        EXPECT_THAT(responses[i].body, Not(IsEmpty())) << "Response " << i << " should not be empty";

        // Parse and verify each response
        json response_json = json::parse(responses[i].body);
        EXPECT_TRUE(response_json.contains("data")) << "Response " << i << " should contain 'data'";
        EXPECT_TRUE(response_json["data"][0].contains("embedding"))
            << "Response " << i << " should have embedding";
    }

    // Verify that embeddings are different for different inputs
    json json0 = json::parse(responses[0].body);
    json json1 = json::parse(responses[1].body);
    json json2 = json::parse(responses[2].body);

    vector<float> emb0 = json0["data"][0]["embedding"].get<vector<float>>();
    vector<float> emb1 = json1["data"][0]["embedding"].get<vector<float>>();
    vector<float> emb2 = json2["data"][0]["embedding"].get<vector<float>>();

    // Check that embeddings have the same dimension
    EXPECT_EQ(emb0.size(), emb1.size()) << "Embeddings should have same dimension";
    EXPECT_EQ(emb1.size(), emb2.size()) << "Embeddings should have same dimension";

    // Check that embeddings are different (not identical)
    bool emb0_eq_emb1 = (emb0 == emb1);
    bool emb1_eq_emb2 = (emb1 == emb2);
    EXPECT_FALSE(emb0_eq_emb1) << "Different texts should produce different embeddings";
    EXPECT_FALSE(emb1_eq_emb2) << "Different texts should produce different embeddings";

    event_loop.join();
}

// Main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    cout << "\n================================================\n";
    cout << "AsyncOpenAIAPI Integration Tests\n";
    cout << "================================================\n";
    cout << "These tests require:\n";
    cout << "- Internet connectivity\n";
    cout << "- Valid OPENAI_API_KEY (env var or .env file)\n";
    cout << "================================================\n\n";

    return RUN_ALL_TESTS();
}
