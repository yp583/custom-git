#include "async_openai_api.hpp"
#include "utils.hpp"
using namespace std;

int main() {
    const char* api_key_env = getenv("OPENAI_API_KEY");
    string api_key = api_key_env ? api_key_env : "";

    if (api_key.empty()) {
        cerr << "Error: OPENAI_API_KEY not found in environment variables" << endl;
        return 1;
    }

    string diff;
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line[0] == '+') {
            diff += "Insertion: ";
        }
        else if (!line.empty() && line[0] == '-') {
            diff += "Deletion: ";
        }
        diff += line + "\n";
    }

    AsyncHTTPSConnection conn;
    AsyncOpenAIAPI openai_api(conn, api_key);

    future<string> msg_future = async_generate_commit_message(openai_api, diff);
    openai_api.run_requests();

    string message = msg_future.get();
    cout << message << endl;

    return 0;
}