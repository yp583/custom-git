#ifndef OPENAI_API_HPP
#define OPENAI_API_HPP

#include "https_api.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using namespace std;

class OpenAIAPI {
  private:
    APIConnection api_connection;
    std::string api_key;
  public:
    OpenAIAPI(const string api_key);
    vector<float> post_embedding(string text);
    vector<float> parse_embedding(const string& response);
    string post_chat(const nlohmann::json& messages, int max_tokens = 100, float temperature = 0.7);
    string parse_chat_response(const string& response);
};

#endif // OPENAI_API_HPP
