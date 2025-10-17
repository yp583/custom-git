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
            {"content", "You are a git commit message generator. Analyze the code changes and generate a concise commit message (max 7-8 words) that describes what was actually modified, added, or fixed in the code. Focus on the technical changes, not meta-commentary. Return only the commit message without quotes or explanations. Examples: 'add HTTP chunked encoding support', 'handle SSL connection errors', 'extract JSON parsing logic'."}
        },
        {
            {"role", "user"},
            {"content", "Generate a commit message for these code changes:\n" + code_changes}
        }
    };
    
    return chat_api.send_chat(messages, 50, 0.3);
}