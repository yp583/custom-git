#include <iostream>
#include <string>
#include <vector>
#include <string>
#include <regex>
#include <cpp-tree-sitter.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

#include "ast.h"
#include "openai_api.h"
#include "utils.h"
#include "hierarchal.h"

struct Chunk {
  string code;
  vector<float> embedding;
  string file;
  int line_number;
  string language;
};

struct DiffFile {
  string filepath;
  string language;
  string insertions;
  string deletions;
};

using namespace std;
using json = nlohmann::json;



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
  
  // Parse git diff to extract file-specific changes
  vector<DiffFile> diff_files;
  DiffFile current_file;
  bool in_file = false;
  bool in_chunk = false;
  
  regex diff_header_regex("^diff --git a/(.*) b/(.*)");
  regex chunk_header_regex("^@@.*@@");
  regex ins_regex("^\\+(?!\\+)(.*)");
  regex del_regex("^\\-(?!\\-)(.*)");
  
  while (getline(cin, line)) {
    smatch match;
    
    // Check for new file header
    if (regex_match(line, match, diff_header_regex)) {
      // Save previous file if exists
      if (in_file) {
        diff_files.push_back(current_file);
      }
      
      // Start new file
      current_file = DiffFile();
      current_file.filepath = match[2]; // Use 'b/' path (after changes)
      current_file.language = detectLanguageFromPath(current_file.filepath);
      in_file = true;
      in_chunk = false;
      continue;
    }
    
    // Check for chunk header (@@)
    if (in_file && regex_match(line, chunk_header_regex)) {
      in_chunk = true;
      continue;
    }
    
    // Process diff lines if we're in a chunk
    if (in_file && in_chunk) {
      if (regex_match(line, match, ins_regex)) {
        current_file.insertions += match[1].str() + "\n";
      } else if (regex_match(line, match, del_regex)) {
        current_file.deletions += match[1].str() + "\n";
      }
    }
  }
  
  // Don't forget the last file
  if (in_file) {
    diff_files.push_back(current_file);
  }

  // Process each file with language-specific parsing
  vector<string> ins_chunks;
  vector<string> del_chunks;
  
  for (const DiffFile& file : diff_files) {
    if (!file.insertions.empty()) {
      ts::Tree ins_tree = codeToTree(file.insertions, file.language);
      vector<string> ins_code_tree = chunkNode(ins_tree.getRootNode(), file.insertions);
      ins_chunks.insert(ins_chunks.end(), ins_code_tree.begin(), ins_code_tree.end());
    }
    
    if (!file.deletions.empty()) {
      ts::Tree del_tree = codeToTree(file.deletions, file.language);
      vector<string> del_code_tree = chunkNode(del_tree.getRootNode(), file.deletions);
      del_chunks.insert(del_chunks.end(), del_code_tree.begin(), del_code_tree.end());
    }
  }

  // for (int i = 0; i < ins_chunks.size(); i++) {
  //   cout << "Insertion:\n" << ins_chunks[i] << endl;
  // }
  // for (int i = 0; i < del_chunks.size(); i++) {
  //   cout << "Deletion:\n" << del_chunks[i] << endl;
  // }

  // Generate embeddings (progress to stderr so it doesn't interfere with JSON output)
  cerr << "Embedding " << ins_chunks.size() << " insertions and " << del_chunks.size() << " deletions" << endl;
  
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
  // Clustering
  hc.cluster(embeddings, dist_thresh);
  vector<vector<int>> clusters = hc.get_clusters();
  
  // Generate commit messages using ChatAPI
  OpenAI_ChatAPI openai_chat_api(api_key);
  
  // Output JSON for shell script to parse
  json output_json = json::array();
  
  for (int i = 0; i < clusters.size(); i++) {
    json cluster_json;
    cluster_json["cluster_id"] = i;
    
    string combined_changes;
    json changes = json::array();
    
    for (int j = 0; j < clusters[i].size(); j++) {
      int idx = clusters[i][j];
      json change_json;
      
      if (idx < ins_chunks.size()) {
        change_json["type"] = "insertion";
        change_json["code"] = ins_chunks[idx];
        combined_changes += "+" + ins_chunks[idx] + "\n";
      } else {
        change_json["type"] = "deletion";
        change_json["code"] = del_chunks[idx - ins_chunks.size()];
        combined_changes += "-" + del_chunks[idx - ins_chunks.size()] + "\n";
      }
      changes.push_back(change_json);
    }
    
    cluster_json["changes"] = changes;
    
    // Generate commit message for this cluster
    string commit_message = openai_chat_api.generate_commit_message(combined_changes);
    cluster_json["commit_message"] = commit_message;
    
    output_json.push_back(cluster_json);
  }
  
  cout << output_json.dump() << endl;
  

  // vector<vector<float>> sim_matrix(embeddings.size(), vector<float>(embeddings.size()));

  // for (int i = 0; i < embeddings.size(); i++) {
  //   for (int j = i + 1; j < embeddings.size(); j++) {
  //     sim_matrix[i][j] = cos_sim(embeddings[i], embeddings[j]);
  //     cout << i << " " << j << " " << sim_matrix[i][j] << endl;
  //   }
  // }

  return 0;
}