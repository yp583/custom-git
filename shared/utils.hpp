#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include "openai_api.hpp"
using namespace std;

float cos_sim(vector<float> a, vector<float> b);
string generate_commit_message(OpenAIAPI& chat_api, const string& code_changes);
vector<float> parse_embedding(const string& response);
#endif // UTILS_HPP