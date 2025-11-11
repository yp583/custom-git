#include "openai_api.hpp"

using namespace std;
using json = nlohmann::json;

OpenAIAPI::OpenAIAPI(const string api_key) 
    : api_connection("api.openai.com", "/v1/embeddings"), api_key(api_key) {}

vector<float> OpenAIAPI::post_embedding(string text) {
    const vector<pair<string, string>> headers = {
        {"Authorization", "Bearer " + this->api_key}, 
        {"Content-Type", "application/json"}
    };
    
    json request_body = {
        {"model", "text-embedding-3-small"},
        {"input", text}
    };
    string body = request_body.dump();
    string raw_response = this->api_connection.post(body, headers);

    vector<float> embedding = this->parse_embedding(raw_response);
    return embedding;
}

vector<float> OpenAIAPI::parse_embedding(const string& response) {
    try {
        json j = json::parse(response);
        vector<float> embedding = j["data"][0]["embedding"].get<vector<float>>();
        return embedding;
    } catch (json::exception& e) {
        cout << "JSON parsing error with response: " << response << endl;
        return vector<float>();
    }
}

// OpenAI Chat API Implementation
OpenAI_ChatAPI::OpenAI_ChatAPI(const string api_key)
    : api_connection("api.openai.com", "/v1/chat/completions"), api_key(api_key) {}

string OpenAIAPI::post_chat(const nlohmann::json& messages, int max_tokens, float temperature) {
    const vector<pair<string, string>> headers = {
        {"Authorization", "Bearer " + this->api_key},
        {"Content-Type", "application/json"}
    };

    json request_body = {
        {"model", "gpt-4o-mini"},
        {"messages", messages},
        {"max_tokens", max_tokens},
        {"temperature", temperature}
    };

    string body = request_body.dump();
    string raw_response = this->api_connection.post(body, headers);
    
    return this->parse_chat_response(raw_response);
}

string OpenAIAPI::parse_chat_response(const string& response) {
    try {
        json j = json::parse(response);
        string message = j["choices"][0]["message"]["content"].get<string>();
        
        // Trim whitespace and remove quotes if present
        size_t start = message.find_first_not_of(" \t\n\r\"");
        size_t end = message.find_last_not_of(" \t\n\r\"");
        
        if (start == string::npos) return "update code";
        
        return message.substr(start, end - start + 1);
    } catch (json::exception& e) {
        cout << "Chat JSON parsing error with response: " << response << endl;
        return "update code"; // fallback message
    }
}

// int main() { 
//     ifstream env_file("./.env");
//     string line;
//     string api_key;
//     while (getline(env_file, line)) {
//       if (line.substr(0, 14) == "OPENAI_API_KEY") {
//           api_key = line.substr(15); //difference from 15 to 14 is because of = sign
//           break;
//       }
//     }
//     if (api_key.empty()) {
//         cerr << "Error: OPENAI_API_KEY not found in .env file" << endl;
//         return 1;
//     }
//     OpenAI_EmbeddingsAPI openai_embeddings_api(api_key); 

//     vector<float> embedding = openai_embeddings_api.post("Hello, world!"); 
    
//     // Now you can use the embedding vector
//     cout << "Embedding size: " << embedding.size() << endl;
//     cout << "First few values: ";
//     for (int i = 0; i < 5 && i < embedding.size(); i++) {
//         cout << embedding[i] << " ";
//     }
//     cout << endl;
    
//     return 0;
// } 