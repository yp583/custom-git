#include "ast.hpp"
#include "openai_api.hpp"
#include "utils.hpp"
#include "hierarchal.hpp"
#include <regex>
#include <set>

struct Chunk {
  string code;
  vector<float> embedding;
  
  string file;
  int line_number;
  string language;
  string type; // "insertion" or "deletion"
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

  string api_key = getenv("OPENAI_API_KEY");
  
  if (api_key.empty()) {
    cerr << "Error: OPENAI_API_KEY not found in .env file or environment variables" << endl;
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

  cerr << "Parsed " << diff_files.size() << " files from git diff" << endl;

  // Process each file with language-specific parsing
  vector<Chunk> all_chunks;
  
  for (const DiffFile& file : diff_files) {
    // Check if this is a text file that should use character-based chunking
    bool useCharacterChunking = isTextFile(file.filepath);
    
    if (!file.insertions.empty()) {
      if (useCharacterChunking) {
        vector<string> ins_text_chunks = chunkByCharacters(file.insertions);
        for (const string& chunk_code : ins_text_chunks) {
          Chunk chunk;
          chunk.code = chunk_code;
          chunk.file = file.filepath;
          chunk.language = file.language;
          chunk.line_number = 0; // Not applicable for text chunks
          chunk.type = "insertion";
          all_chunks.push_back(chunk);
        }
      } else {
        ts::Tree ins_tree = codeToTree(file.insertions, file.language);
        vector<string> ins_code_tree = chunkNode(ins_tree.getRootNode(), file.insertions);
        for (const string& chunk_code : ins_code_tree) {
          Chunk chunk;
          chunk.code = chunk_code;
          chunk.file = file.filepath;
          chunk.language = file.language;
          chunk.line_number = 0; // Could be enhanced to track actual line numbers
          chunk.type = "insertion";
          all_chunks.push_back(chunk);
        }
      }
    }
    
    if (!file.deletions.empty()) {
      if (useCharacterChunking) {
        vector<string> del_text_chunks = chunkByCharacters(file.deletions);
        for (const string& chunk_code : del_text_chunks) {
          Chunk chunk;
          chunk.code = chunk_code;
          chunk.file = file.filepath;
          chunk.language = file.language;
          chunk.line_number = 0; // Not applicable for text chunks
          chunk.type = "deletion";
          all_chunks.push_back(chunk);
        }
      } else {
        ts::Tree del_tree = codeToTree(file.deletions, file.language);
        vector<string> del_code_tree = chunkNode(del_tree.getRootNode(), file.deletions);
        for (const string& chunk_code : del_code_tree) {
          Chunk chunk;
          chunk.code = chunk_code;
          chunk.file = file.filepath;
          chunk.language = file.language;
          chunk.line_number = 0; // Could be enhanced to track actual line numbers
          chunk.type = "deletion";
          all_chunks.push_back(chunk);
        }
      }
    }
  }

  // Generate embeddings (progress to stderr so it doesn't interfere with JSON output)
  cerr << "Embedding " << all_chunks.size() << " chunks" << endl;

  for (int i = 0; i < all_chunks.size(); i++) {
    cerr << "Embedding chunk " << (i+1) << "/" << all_chunks.size() << endl;
    cerr << "Chunk preview: " << all_chunks[i].code.substr(0, min((size_t)50, all_chunks[i].code.size())) << "..." << endl;
    vector<float> response = openai_embeddings_api.post(all_chunks[i].code);
    cerr << "Got embedding with " << response.size() << " dimensions" << endl;
    all_chunks[i].embedding = response;
    embeddings.push_back(response);
  }

  cerr << "Starting hierarchical clustering..." << endl;
  HierachicalClustering hc;

  float dist_thresh = 0.5;
  if (argc > 1) {
    dist_thresh = stof(argv[1]);
  }
  cerr << "Distance threshold: " << dist_thresh << endl;

  // Clustering
  hc.cluster(embeddings, dist_thresh);
  vector<vector<int>> clusters = hc.get_clusters();
  cerr << "Clustering complete. Found " << clusters.size() << " clusters" << endl;

  // Generate commit messages using ChatAPI
  cerr << "Initializing Chat API for commit message generation..." << endl;
  OpenAI_ChatAPI openai_chat_api(api_key);
  
  // Output JSON for shell script to parse
  json output_json = json::array();
  
  for (int i = 0; i < clusters.size(); i++) {
    cerr << "Processing cluster " << (i+1) << "/" << clusters.size() << " with " << clusters[i].size() << " chunks" << endl;
    json cluster_json;
    cluster_json["cluster_id"] = i;

    string combined_changes;
    json changes = json::array();
    set<string> affected_files; // Track unique files in this cluster

    for (int j = 0; j < clusters[i].size(); j++) {
      int idx = clusters[i][j];
      json change_json;
      
      const Chunk& chunk = all_chunks[idx];
      change_json["type"] = chunk.type; // "insertion" or "deletion"
      change_json["code"] = chunk.code;
      change_json["file"] = chunk.file;
      change_json["language"] = chunk.language;
      
      // Add file to affected files set
      affected_files.insert(chunk.file);
      
      // Format for AI with file context
      combined_changes += "File: " + chunk.file + " (" + chunk.type + ")\n";
      if (chunk.type == "insertion") {
        combined_changes += "+" + chunk.code + "\n\n";
      } else {
        combined_changes += "-" + chunk.code + "\n\n";
      }
      
      changes.push_back(change_json);
    }
    
    cluster_json["changes"] = changes;
    cluster_json["affected_files"] = vector<string>(affected_files.begin(), affected_files.end());
    
    // Generate commit message with file context
    string file_context = "Files changed: ";
    for (const string& file : affected_files) {
      file_context += file + ", ";
    }
    file_context = file_context.substr(0, file_context.length() - 2); // Remove trailing comma
    
    cerr << "Generating commit message for cluster " << (i+1) << "..." << endl;
    string commit_message = openai_chat_api.generate_commit_message(file_context + "\n\nChanges:\n" + combined_changes);
    cerr << "Generated commit message: " << commit_message << endl;
    cluster_json["commit_message"] = commit_message;

    output_json.push_back(cluster_json);
  }

  cerr << "Outputting final JSON..." << endl;
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