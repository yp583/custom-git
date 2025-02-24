#include <fstream>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

#include "openai_api.h"

using namespace std;
using json = nlohmann::json;

OpenAI_EmbeddingsAPI::OpenAI_EmbeddingsAPI(const string api_key) 
    : api_connection("api.openai.com", "/v1/embeddings"), api_key(api_key) {}

vector<float> OpenAI_EmbeddingsAPI::post(string text) {
    const vector<pair<string, string>> headers = {
        {"Authorization", "Bearer " + this->api_key}, 
        {"Content-Type", "application/json"}
    };
    
    json request_body = {
        {"model", "text-embedding-3-small"},
        {"input", text}
    };
    string body = request_body.dump();
    cout << "body: " << body << endl;
    string raw_response = this->api_connection.post(body, headers);
    cout << "raw_response: " << raw_response << endl;
    
    size_t start = raw_response.find("\r\n\r\n") + 4;
    size_t end = raw_response.find("\r\n", start);

    string embedding_str = raw_response.substr(start, end - start);

    vector<float> embedding = this->parse_embedding(embedding_str);
    return embedding;
}

vector<float> OpenAI_EmbeddingsAPI::parse_embedding(const string& response) {
    try {
        json j = json::parse(response);
        vector<float> embedding = j["data"][0]["embedding"].get<vector<float>>();
        return embedding;
    } catch (json::exception& e) {
        cerr << "JSON parsing error with response: " << response << endl;
        return vector<float>();
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