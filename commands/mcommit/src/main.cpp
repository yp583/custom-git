#include "openai_api.hpp"
#include "utils.hpp"
using namespace std;

int main() {
    string api_key = getenv("OPENAI_API_KEY");

    if (api_key.empty()) {
        cerr << "Error: OPENAI_API_KEY not found in .env file or environment variables" << endl;
        return 1;
    } 

    OpenAI_ChatAPI openai_chat_api(api_key);

    string diff;

    string line;
    while (getline(cin, line)) {
        diff += line + "\n";
    }

    string message = generate_commit_message(openai_chat_api, diff);
    cout << message;
}