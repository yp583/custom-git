#include "https_api.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

using namespace std;

class OpenAI_EmbeddingsAPI {
  private:
    APIConnection api_connection;
    std::string api_key;
  public:
    OpenAI_EmbeddingsAPI(const string api_key);
    vector<float> post(string text);
    vector<float> parse_embedding(const string& response);
};

class OpenAI_ChatAPI {
  private:
    APIConnection api_connection;
    std::string api_key;
  public:
    OpenAI_ChatAPI(const string api_key);
    string generate_commit_message(const string& code_changes);
    string parse_chat_response(const string& response);
};
