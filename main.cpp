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

struct Chunk {
  string code;
  vector<float> embedding;
  string file;
  int line_number;
};

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
  
  regex chunk_header_regex("(@@).+(@@)");
  sregex_iterator diff_header_iter = sregex_iterator(git_diff.begin(), git_diff.end(), chunk_header_regex);
  sregex_iterator diff_header_end = sregex_iterator();
  smatch header_match;

  vector<string> diff_chunks;
  string chunk;
  string new_file_diff_header = "diff --git";
  bool in_diff_header = false;
  int line_count = 0;
  while (getline(cin, line)) {

    if (line.substr(0, new_file_diff_header.length()) == new_file_diff_header) {
      in_diff_header = true;
      if (!chunk.empty()) {
        diff_chunks.push_back(chunk);
      }
      chunk = "";
    }
    else if (in_diff_header && regex_search(line, header_match, chunk_header_regex)) {
      if (!chunk.empty()) {
        diff_chunks.push_back(chunk);
      }
      line = header_match.suffix();
      chunk = "";
      in_diff_header = false;
    }
    if (!in_diff_header) {
      chunk += line + "\n";
    }
  }
  diff_chunks.push_back(chunk);
  regex ins_regex("\\n\\+(?!\\+)[^\\n]+");
  regex del_regex("\\n\\-(?!\\-)[^\\n]+");
  sregex_iterator end = sregex_iterator();

  vector<string> ins_chunks;
  vector<string> del_chunks;

  for (string diff_chunk : diff_chunks) {
    sregex_iterator ins_iter = sregex_iterator(diff_chunk.begin(), diff_chunk.end(), ins_regex);
    sregex_iterator del_iter = sregex_iterator(diff_chunk.begin(), diff_chunk.end(), del_regex);


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

    vector<string> ins_code_tree = chunkNode(ins_tree.getRootNode(), insertions);
    vector<string> del_code_tree = chunkNode(del_tree.getRootNode(), deletions);
    ins_chunks.insert(ins_chunks.end(), ins_code_tree.begin(), ins_code_tree.end());
    del_chunks.insert(del_chunks.end(), del_code_tree.begin(), del_code_tree.end());
  }

  // for (int i = 0; i < ins_chunks.size(); i++) {
  //   cout << "Insertion:\n" << ins_chunks[i] << endl;
  // }
  // for (int i = 0; i < del_chunks.size(); i++) {
  //   cout << "Deletion:\n" << del_chunks[i] << endl;
  // }

  cout << "Embedding " << ins_chunks.size() << " insertions and " << del_chunks.size() << " deletions" << endl;
  cout << "Total chunks: " << ins_chunks.size() + del_chunks.size() << endl;
  
  for (int i = 0; i < ins_chunks.size(); i++) {
    vector<float> response = openai_embeddings_api.post( ins_chunks[i]);
    embeddings.push_back(response);
  }
  for (int i = 0; i < del_chunks.size(); i++) {
    vector<float> response = openai_embeddings_api.post(del_chunks[i]);
    embeddings.push_back(response);
  }

  HierachicalClustering hc;

  float dist_thresh = 0.5;
  if (argc > 1) {
    dist_thresh = stof(argv[1]);
  }
  cout << "Clustering with distance threshold: " << dist_thresh << endl;
  hc.cluster(embeddings, dist_thresh);
  vector<vector<int>> clusters = hc.get_clusters();
  for (int i = 0; i < clusters.size(); i++) {
    cout << "Cluster " << i << ": " << endl;
    for (int j = 0; j < clusters[i].size(); j++) {
      int idx = clusters[i][j];
      if (idx < ins_chunks.size()) {
        cout << "Insertion: " << ins_chunks[idx] << endl;
      } else {
        cout << "Deletion: " << del_chunks[idx - ins_chunks.size()] << endl;
      }
    }
  }
  

  // vector<vector<float>> sim_matrix(embeddings.size(), vector<float>(embeddings.size()));

  // for (int i = 0; i < embeddings.size(); i++) {
  //   for (int j = i + 1; j < embeddings.size(); j++) {
  //     sim_matrix[i][j] = cos_sim(embeddings[i], embeddings[j]);
  //     cout << i << " " << j << " " << sim_matrix[i][j] << endl;
  //   }
  // }

  return 0;
}