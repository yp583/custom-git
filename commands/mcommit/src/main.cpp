#include "async_openai_api.hpp"
#include "utils.hpp"
using namespace std;

int main() {
    const char* api_key_env = getenv("OPENAI_API_KEY");
    string api_key = api_key_env ? api_key_env : "";

    // If not in env, check git config custom.openai_api_key
    if (api_key.empty()) {
        FILE* pipe = popen("git config --get custom.openai_api_key 2>/dev/null", "r");
        if (pipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                api_key = buffer;
                // Remove trailing newline
                if (!api_key.empty() && api_key.back() == '\n') {
                    api_key.pop_back();
                }
            }
            pclose(pipe);
        }
    }

    if (api_key.empty()) {
        cerr << "Error: OPENAI_API_KEY not found in environment or git config (custom.openai_api_key)" << endl;
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