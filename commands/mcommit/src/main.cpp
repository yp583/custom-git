#include "openai_api.hpp"
using namespace std;

int main() {
    string apikey = getenv("OPENAI_API_KEY");
    cout << apikey << endl;
    // OpenAI_ChatAPI openai_embeddings_api(api_key);
}