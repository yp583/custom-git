#include <iostream>
#include <string>
#include <vector>
#include <string>
#include <regex>
#include <cpp-tree-sitter.h>
#include <fstream>
#include <chrono>
#include <thread>

#include "ast.h"
#include "openai_api.h"
#include "utils.h"

using namespace std;



int main() {
  string line;
  string git_diff;
  while (getline(cin, line)) {
    git_diff += line + "\n";
  }
  regex ins_regex("\n\\+[^+].+");
  sregex_iterator ins_iter = sregex_iterator(git_diff.begin(), git_diff.end(), ins_regex);
  sregex_iterator end = sregex_iterator();

  regex del_regex("\n\\-[^-].+");
  sregex_iterator del_iter = sregex_iterator(git_diff.begin(), git_diff.end(), del_regex);

  string insertions;
  string deletions;

  for (sregex_iterator it = ins_iter; it != end; it++) {
      smatch match = *it;
      insertions += match.str();
  }
  for (sregex_iterator it = del_iter; it != end; it++) {
      smatch match = *it;
      deletions += match.str();
  }

  ts::Tree ins_tree = codeToTree(insertions);
  ts::Tree del_tree = codeToTree(deletions);

  vector<string> ins_chunks = chunkNode(ins_tree.getRootNode(), insertions);
  vector<string> del_chunks = chunkNode(del_tree.getRootNode(), deletions);


  ifstream env_file("./.env");
  string api_key;
  while (getline(env_file, line)) {
    if (line.substr(0, 14) == "OPENAI_API_KEY") {
      api_key = line.substr(15);
      break;
    }
  }
  if (api_key.empty()) {
    cerr << "Error: OPENAI_API_KEY not found in .env file" << endl;
    return 1;
  }
  OpenAI_EmbeddingsAPI openai_embeddings_api(api_key);
  vector<vector<float>> embeddings;
  for (int i = 0; i < ins_chunks.size(); i++) {
    vector<float> response = openai_embeddings_api.post("ADDED CODE WAS:\n" + ins_chunks[i]);
    embeddings.push_back(response);
  }
  for (int i = 0; i < del_chunks.size(); i++) {
    vector<float> response = openai_embeddings_api.post("REMOVED CODE WAS:\n" + del_chunks[i]);
    embeddings.push_back(response);
  }

  vector<vector<float>> sim_matrix(embeddings.size(), vector<float>(embeddings.size()));

  for (int i = 0; i < embeddings.size(); i++) {
    for (int j = i + 1; j < embeddings.size(); j++) {
      sim_matrix[i][j] = cos_sim(embeddings[i], embeddings[j]);
      cout << i << " " << j << " " << sim_matrix[i][j] << endl;
    }
  }

  return 0;
}