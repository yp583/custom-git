#include "ast.hpp"
#include "async_openai_api.hpp"
#include "utils.hpp"
#include "hierarchal.hpp"
#include "diffreader.hpp"
#include <regex>
#include <set>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>
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

  const char* api_key_env = getenv("OPENAI_API_KEY");
  string api_key = api_key_env ? api_key_env : "";
  
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

  AsyncHTTPSConnection conn;
  AsyncOpenAIAPI openai_api(conn, api_key);
  vector<future<HTTPSResponse>> embedding_resp_futures;

  cout << "Adding Embedding requests to the queue" << endl;

  for (size_t i = 0; i < ins_chunks.size(); i++){
    string content = combineContent(ins_chunks[i]);
    content = "===== CODE THAT HAS BEEN ADDED =====\n\n" + content;

    future<HTTPSResponse> resp_future = openai_api.async_embedding(content);
    embedding_resp_futures.push_back(move(resp_future));
  }
  for (size_t i = 0; i < del_chunks.size(); i++){
    string content = combineContent(del_chunks[i]);
    content = "===== CODE THAT HAS BEEN DELETED =====\n\n" + content;

    future<HTTPSResponse> resp_future = openai_api.async_embedding(content);
    embedding_resp_futures.push_back(move(resp_future));
  }

  openai_api.run_requests();

  vector<vector<float>> embeddings;
  for (future<HTTPSResponse>& resp: embedding_resp_futures) {
    vector<float> embedding;
    try {
      embedding = parse_embedding(resp.get().body);
    }
    catch (const exception& e) {
      embedding = {};
    }
    embeddings.push_back(embedding);
    cout << "Done with Embedding Job" << endl;
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

  // Write each DiffChunk as a separate patch file
  filesystem::create_directories("/tmp/patches");
  int patch_num = 1;

  for (size_t i = 0; i < clusters.size(); i++) {
    const vector<int>& cluster = clusters[i];

    cout << "Cluster " << (i + 1) << ":" << endl;
    for (int idx: cluster) {
      DiffChunk chunk;
      if (idx < ins_chunks.size()) {
        chunk = ins_chunks[idx];
      } else {
        chunk = del_chunks[idx - ins_chunks.size()];
      }

      string patch_path = "/tmp/patches/patch_" + to_string(patch_num++) + ".patch";
      ofstream patch_file(patch_path);
      string patch = createPatch(chunk);
      patch_file << patch;
      patch_file.close();

      cout << "  " << patch_path << endl;
      cout << patch << endl;
    }
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