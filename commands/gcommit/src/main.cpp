#include "ast.hpp"
#include "openai_api.hpp"
#include "utils.hpp"
#include "hierarchal.hpp"
#include "diffreader.hpp"
#include <regex>
#include <set>
#include <vector>
struct Chunk {
  string code;
  vector<float> embedding;
  
  string file;
  int line_number;
  string language;
  string type; // "insertion" or "deletion"
};

using namespace std;
using json = nlohmann::json;



int main(int argc, char *argv[]) {

  string api_key = getenv("OPENAI_API_KEY");
  
  if (api_key.empty()) {
    cout << "Error: OPENAI_API_KEY not found in .env file or environment variables" << endl;
    return 1;
  }


  //handles reading from cin too
  DiffReader dr(cin);
  dr.ingestDiff();

  cout << "Parsed " << dr.getFiles().size() << " files from git diff" << endl;

  vector<DiffChunk> ins_chunks;
  vector<DiffChunk> del_chunks;



  for (const DiffFile& file : dr.getFiles()) {
    string language = detectLanguageFromPath(file.filepath);
    DiffChunk file_ins = getDiffContent(file, {EQ, INSERTION});
    DiffChunk file_del = getDiffContent(file, {EQ, DELETION});

    vector<DiffChunk> file_ins_chunks;
    vector<DiffChunk> file_del_chunks;

    if (language != "text") { //use ast for non text files
      string file_ins_content = combineContent(file_ins);
      string file_del_content = combineContent(file_del);
      ts::Tree ins_tree = codeToTree(file_ins_content, language);
      ts::Tree del_tree = codeToTree(file_del_content, language);

      file_ins_chunks = chunkDiff(ins_tree.getRootNode(), file_ins);
      file_del_chunks = chunkDiff(del_tree.getRootNode(), file_del);
      
      ins_chunks.insert(ins_chunks.end(), file_ins_chunks.begin(), file_ins_chunks.end());
      del_chunks.insert(del_chunks.end(), file_del_chunks.begin(), file_del_chunks.end());
    }
    else {
      file_ins_chunks = chunkByLines(file_ins);
      file_del_chunks = chunkByLines(file_del);
      
      ins_chunks.insert(ins_chunks.end(), file_ins_chunks.begin(), file_ins_chunks.end());
      del_chunks.insert(del_chunks.end(), file_del_chunks.begin(), file_del_chunks.end());
    }
  }

  // // Generate embeddings (progress to stderr so it doesn't interfere with JSON output)
  // cerr << "Embedding " << all_chunks.size() << " chunks" << endl;

  OpenAI_EmbeddingsAPI openai_embeddings_api(api_key);
  vector<vector<float>> embeddings;

  for (size_t i = 0; i < ins_chunks.size(); i++){
    string content = combineContent(ins_chunks[i]);
    content = "===== CODE THAT HAS BEEN ADDED =====\n\n" + content;

    vector<float> response = openai_embeddings_api.post(content);
    embeddings.push_back(response);
  }
  for (size_t i = 0; i < del_chunks.size(); i++){
    string content = combineContent(del_chunks[i]);
    content = "===== CODE THAT HAS BEEN DELETED =====\n\n" + content;

    vector<float> response = openai_embeddings_api.post(content);
    embeddings.push_back(response);
  }

  HierachicalClustering hc;

  float dist_thresh = 0.5;
  if (argc > 1) {
    dist_thresh = stof(argv[1]);
  }
  cout << "Starting hierarchical clustering with distance threshold of " << dist_thresh << " ..." << endl;

  // Clustering
  hc.cluster(embeddings, dist_thresh);
  vector<vector<int>> clusters = hc.get_clusters();
  cout << "Clustering complete. Found " << clusters.size() << " clusters" << endl;

  for (size_t i = 0; i < clusters.size(); i++) {
    const vector<int>& cluster = clusters[i];
    vector<DiffChunk> cluster_chunks;
    
    for (int idx: cluster) {
      if (idx < ins_chunks.size()) {
        cluster_chunks.push_back(ins_chunks[idx]);
      } else {
        cluster_chunks.push_back(del_chunks[idx - ins_chunks.size()]);
      }
    }
    
    cout << "Chunk " << (i + 1) << ":" << endl;
    for (const DiffChunk& chunk : cluster_chunks) {
      cout << combineContent(chunk) << endl;
    }
    cout << endl;
  }

  // // Generate commit messages using ChatAPI
  // cerr << "Initializing Chat API for commit message generation..." << endl;
  // OpenAI_ChatAPI openai_chat_api(api_key);
  
  // // Output JSON for shell script to parse
  // json output_json = json::array();
  
  // for (int i = 0; i < clusters.size(); i++) {
  //   cerr << "Processing cluster " << (i+1) << "/" << clusters.size() << " with " << clusters[i].size() << " chunks" << endl;
  //   json cluster_json;
  //   cluster_json["cluster_id"] = i;

  //   string combined_changes;
  //   json changes = json::array();
  //   set<string> affected_files; // Track unique files in this cluster

  //   for (int j = 0; j < clusters[i].size(); j++) {
  //     int idx = clusters[i][j];
  //     json change_json;
      
  //     const Chunk& chunk = all_chunks[idx];
  //     change_json["type"] = chunk.type; // "insertion" or "deletion"
  //     change_json["code"] = chunk.code;
  //     change_json["file"] = chunk.file;
  //     change_json["language"] = chunk.language;
      
  //     // Add file to affected files set
  //     affected_files.insert(chunk.file);
      
  //     // Format for AI with file context
  //     combined_changes += "File: " + chunk.file + " (" + chunk.type + ")\n";
  //     if (chunk.type == "insertion") {
  //       combined_changes += "+" + chunk.code + "\n\n";
  //     } else {
  //       combined_changes += "-" + chunk.code + "\n\n";
  //     }
      
  //     changes.push_back(change_json);
  //   }
    
  //   cluster_json["changes"] = changes;
  //   cluster_json["affected_files"] = vector<string>(affected_files.begin(), affected_files.end());
    
  //   // Generate commit message with file context
  //   string file_context = "Files changed: ";
  //   for (const string& file : affected_files) {
  //     file_context += file + ", ";
  //   }
  //   file_context = file_context.substr(0, file_context.length() - 2); // Remove trailing comma
    
  //   cerr << "Generating commit message for cluster " << (i+1) << "..." << endl;
  //   string commit_message = generate_commit_message(openai_chat_api, file_context + "\n\nChanges:\n" + combined_changes);
  //   cerr << "Generated commit message: " << commit_message << endl;
  //   cluster_json["commit_message"] = commit_message;

  //   output_json.push_back(cluster_json);
  // }

  // cerr << "Outputting final JSON..." << endl;
  // cout << output_json.dump() << endl;

  // return 0;
}