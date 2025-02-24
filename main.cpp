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
#include "hierarchal.h"

using namespace std;



int main(int argc, char *argv[]) {
  string line;
  string git_diff;

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
  vector<string> git_diff_lines;

  while (getline(cin, line)) {
    git_diff += line + "\n";
    // git_diff_lines.push_back(line);
    // embeddings.push_back(openai_embeddings_api.post(line));
  }
  regex chunk_header_regex("(@@).+(@@)");
  
  vector<string> diff_chunks;
  sregex_iterator diff_header_iter = sregex_iterator(git_diff.begin(), git_diff.end(), chunk_header_regex);
  sregex_iterator diff_header_end = sregex_iterator();

  auto ahead = diff_header_iter;
  ahead++;
  while (diff_header_iter != diff_header_end) {
    smatch match = *diff_header_iter;
    int chunk_header_idx = match.position();
    int chunk_header_len = match.str().length();
    diff_header_iter++;
    ahead++;
    int next_chunk_header_idx = diff_header_iter->position();
    string chunk_body = git_diff.substr(chunk_header_idx + chunk_header_len, next_chunk_header_idx - chunk_header_idx - chunk_header_len);
    
    size_t last_pos = string::npos;
    if (ahead != diff_header_end) {
      for (int i = 0; i < 5; i++) {
          last_pos = chunk_body.rfind('\n', last_pos - 1);
          if (last_pos == string::npos) break;
      }
      if (last_pos != string::npos) {
          chunk_body = chunk_body.substr(0, last_pos);
      }
    }
    
    diff_chunks.push_back(chunk_body);
  }

  for (string chunk : diff_chunks) {
    cout << "Chunk: " << chunk << endl;
  }

  regex ins_regex("\\n\\+(?!\\+)[^\\n]+");
  sregex_iterator ins_iter = sregex_iterator(git_diff.begin(), git_diff.end(), ins_regex);
  sregex_iterator end = sregex_iterator();

  regex del_regex("\\n\\-(?!\\-)[^\\n]+");
  sregex_iterator del_iter = sregex_iterator(git_diff.begin(), git_diff.end(), del_regex);

  string insertions;
  string deletions;

  for (sregex_iterator it = ins_iter; it != end; it++) {
      smatch match = *it;
      string match_str = match.str().substr(2);
      insertions += match_str + "\n";
  }
  for (sregex_iterator it = del_iter; it != end; it++) {
      smatch match = *it;
      string match_str = match.str().substr(2);
      deletions += match_str + "\n";
  }

  ts::Tree ins_tree = codeToTree(insertions, "cpp");
  ts::Tree del_tree = codeToTree(deletions, "cpp");

  vector<string> ins_chunks = chunkNode(ins_tree.getRootNode(), insertions);
  vector<string> del_chunks = chunkNode(del_tree.getRootNode(), deletions);

  // for (int i = 0; i < ins_chunks.size(); i++) {
  //   cout << "Insertion:\n" << ins_chunks[i] << endl;
  // }
  // for (int i = 0; i < del_chunks.size(); i++) {
  //   cout << "Deletion:\n" << del_chunks[i] << endl;
  // }
  
  // for (int i = 0; i < ins_chunks.size(); i++) {
  //   vector<float> response = openai_embeddings_api.post( ins_chunks[i]);
  //   embeddings.push_back(response);
  // }
  // for (int i = 0; i < del_chunks.size(); i++) {
  //   vector<float> response = openai_embeddings_api.post(del_chunks[i]);
  //   embeddings.push_back(response);
  // }

  // HierachicalClustering hc;

  // float dist_thresh = 0.5;
  // if (argc > 1) {
  //   dist_thresh = stof(argv[1]);
  // }
  // hc.cluster(embeddings, dist_thresh);
  // vector<vector<int>> clusters = hc.get_clusters();
  // for (int i = 0; i < clusters.size(); i++) {
  //   cout << "Cluster " << i << ": " << endl;
  //   for (int j = 0; j < clusters[i].size(); j++) {
  //     int idx = clusters[i][j];
  //     if (idx < ins_chunks.size()) {
  //       cout << "Insertion: " << ins_chunks[idx] << endl;
  //     } else {
  //       cout << "Deletion: " << del_chunks[idx - ins_chunks.size()] << endl;
  //     }
  //   }
  // }
  

  // vector<vector<float>> sim_matrix(embeddings.size(), vector<float>(embeddings.size()));

  // for (int i = 0; i < embeddings.size(); i++) {
  //   for (int j = i + 1; j < embeddings.size(); j++) {
  //     sim_matrix[i][j] = cos_sim(embeddings[i], embeddings[j]);
  //     cout << i << " " << j << " " << sim_matrix[i][j] << endl;
  //   }
  // }

  return 0;
}