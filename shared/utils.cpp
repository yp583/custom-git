#include "utils.hpp"

float cos_sim(vector<float> a, vector<float> b) {
  float dot = 0.0;
  for (size_t i = 0; i < a.size(); i++) {
    dot += a[i] * b[i];
  }
  //assuming vectors are normalized
  return dot; 
}

string generate_commit_message(OpenAI_ChatAPI& chat_api, const string& code_changes) {
    nlohmann::json messages = {
        {
            {"role", "system"},
            {"content", "You are a git commit message generator. Analyze the code changes and generate a concise commit message that describes what was actually modified, added, or fixed in the code. Use conventional commit style (feat:, fix:, refactor:, docs:, etc.). Focus on the technical changes, not meta-commentary. Return ONLY the commit message, no quotes or explanations. Examples: 'feat: add HTTP chunked encoding support', 'fix: handle SSL connection errors', 'refactor: extract JSON parsing logic'."}
        },
        {
            {"role", "user"},
            {"content", "Generate a commit message for these code changes:\n" + code_changes}
        }
    };
    
    return chat_api.send_chat(messages, 50, 0.3);
}