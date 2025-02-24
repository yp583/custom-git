#include "https_api.h"
#include <string>
#include <vector>
#include <utility>
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
