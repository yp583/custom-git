#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "openai_api.hpp"
using namespace std;

float cos_sim(vector<float> a, vector<float> b);
string generate_commit_message(OpenAI_ChatAPI& chat_api, const string& code_changes);

#endif // UTILS_HPP