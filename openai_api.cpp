#include "https_api.cpp"
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

class OpenAI_EmbeddingsAPI {
  private:
    APIConnection api_connection;
    string api_key;
  public:
    OpenAI_EmbeddingsAPI(const string api_key) : api_connection("api.openai.com", "/v1/embeddings"), api_key(api_key) {}

    string post(string text) {
      const vector<pair<string, string>> headers = {{"Authorization", "Bearer " + this->api_key}, {"Content-Type", "application/json"}};
      string body = "{\"model\": \"text-embedding-3-small\",\"input\": \"" + text + "\"}";
      string response = this->api_connection.post(body, headers);
      return response;
    }
    
};
 
int main() { 
    ifstream env_file("./.env");
    string line;
    string api_key;
    while (getline(env_file, line)) {
      if (line.substr(0, 14) == "OPENAI_API_KEY") {
          api_key = line.substr(15); //difference from 15 to 14 is because of = sign
          break;
      }
    }
    if (api_key.empty()) {
        cerr << "Error: OPENAI_API_KEY not found in .env file" << endl;
        return 1;
    }
    OpenAI_EmbeddingsAPI openai_embeddings_api(api_key); 

    string response = openai_embeddings_api.post("Hello, world!"); 
    cout << response << endl;
    return 0; 
} 