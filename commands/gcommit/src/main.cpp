#include "ast.hpp"
#include "async_openai_api.hpp"
#include "utils.hpp"
#include "hierarchal.hpp"
#include "diffreader.hpp"
#include "umap.hpp"
#include <regex>
#include <set>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;

struct ClusteredCommit {
  vector<string> patch_paths;
  string commit_message;

  json to_json() const {
    return json{
      {"patch_paths", patch_paths},
      {"commit_message", commit_message}
    };
  }
};


int main(int argc, char *argv[]) {
  // Parse arguments: [-d threshold] [-i] [-v|-vv]
  float dist_thresh = 0.5;
  int verbose = 0;
  bool interactive = false;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "-vv") {
      verbose = 2;
    } else if (arg == "-v") {
      verbose = 1;
    } else if (arg == "-i") {
      interactive = true;
    } else if (arg == "-d") {
      // -d requires a following argument for threshold
      if (i + 1 < argc) {
        try {
          dist_thresh = stof(argv[++i]);
        } catch (...) {
          cout << "Error: -d requires a numeric threshold value" << endl;
          return 1;
        }
      } else {
        cout << "Error: -d requires a threshold value" << endl;
        return 1;
      }
    } else {
      // Legacy: assume bare number is threshold for backwards compatibility
      try {
        dist_thresh = stof(arg);
      } catch (...) {
        cout << "Usage: " << argv[0] << " [-d threshold] [-i] [-v|-vv]" << endl;
        return 1;
      }
    }
  }

  const char* api_key_env = getenv("OPENAI_API_KEY");
  string api_key = api_key_env ? api_key_env : "";

  if (api_key.empty()) {
    cout << "Error: OPENAI_API_KEY not found in .env file or environment variables" << endl;
    return 1;
  }


  //handles reading from cin too
  DiffReader dr(cin);
  dr.ingestDiff();

  if (verbose >= 1) cout << "Parsed " << dr.getChunks().size() << " chunks from git diff" << endl;

  vector<DiffChunk> all_chunks;

  for (const DiffChunk& chunk : dr.getChunks()) {
    string language = detectLanguageFromPath(chunk.filepath);

    vector<DiffChunk> file_chunks;

    if (language != "text") { //use ast for non text files
      string file_content = combineContent(chunk);
      ts::Tree tree = codeToTree(file_content, language);
      file_chunks = chunkDiff(tree.getRootNode(), chunk);
    }
    else {
      file_chunks = chunkByLines(chunk);
    }

    all_chunks.insert(all_chunks.end(), file_chunks.begin(), file_chunks.end());
  }

  AsyncHTTPSConnection conn(verbose);
  AsyncOpenAIAPI openai_api(conn, api_key);
  vector<future<HTTPSResponse>> embedding_resp_futures;

  if (verbose >= 1) cout << "Adding Embedding requests to the queue" << endl;

  const size_t MAX_EMBEDDING_CHARS = 16000;
  for (size_t i = 0; i < all_chunks.size(); i++){
    string content = combineContent(all_chunks[i]);
    if (content.size() > MAX_EMBEDDING_CHARS) {
      content = content.substr(0, MAX_EMBEDDING_CHARS);
    }

    future<HTTPSResponse> resp_future = openai_api.async_embedding(content);
    embedding_resp_futures.push_back(std::move(resp_future));
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
    if (verbose >= 1) cout << "Done with Embedding Job" << endl;
  }

  HierachicalClustering hc;

  if (verbose >= 1) cout << "Starting hierarchical clustering with distance threshold of " << dist_thresh << " ..." << endl;

  // Clustering
  hc.cluster(embeddings, dist_thresh);
  vector<vector<int>> clusters = hc.get_clusters();
  if (verbose >= 1) cout << "Clustering complete. Found " << clusters.size() << " clusters" << endl;

  // UMAP computation for interactive visualization
  vector<UmapPoint> umap_points;
  if (interactive) {
    if (verbose >= 1) cout << "Running UMAP dimensionality reduction..." << endl;
    umap_points = compute_umap(embeddings);
    if (verbose >= 1) cout << "UMAP complete." << endl;
  }

  // Build chunk_to_cluster lookup
  vector<int> chunk_to_cluster(all_chunks.size(), -1);
  for (size_t i = 0; i < clusters.size(); i++) {
    for (int idx : clusters[i]) {
      chunk_to_cluster[idx] = static_cast<int>(i);
    }
  }

  vector<DiffChunk> all_cluster_chunks;
  vector<size_t> cluster_end_idx;
  

  for (size_t i = 0; i < clusters.size(); i++) {
    const vector<int>& cluster = clusters[i];

    if (verbose >= 1) cout << "Cluster " << (i + 1) << ":" << endl;

    // Collect chunks for this cluster
    for (int idx: cluster) {
      all_cluster_chunks.push_back(all_chunks[idx]);
    }
    size_t prev_end = cluster_end_idx.empty() ? 0 : cluster_end_idx.back();
    cluster_end_idx.push_back(prev_end + cluster.size());
  }

  vector<string> patches = createPatches(all_cluster_chunks);
  vector<vector<string>> clusters_patch_paths;

  // Write patches to cluster folders
  for (size_t i = 0; i < cluster_end_idx.size(); i++) {
    clusters_patch_paths.push_back(vector<string>());  // New vector for each cluster
    string cluster_dir = "/tmp/patches/cluster_" + to_string(i);
    filesystem::create_directories(cluster_dir);

    size_t start_idx = (i == 0) ? 0 : cluster_end_idx[i - 1];
    size_t end_idx = cluster_end_idx[i];

    int patch_num = 0;
    for (size_t j = start_idx; j < end_idx && j < patches.size(); j++) {
      // Skip empty patches (e.g., chunks with only NO_NEWLINE markers)
      if (patches[j].empty()) {
        if (verbose >= 1) cout << "Skipping empty patch at index " << j << endl;
        continue;
      }
      string patch_path = cluster_dir + "/patch_" + to_string(patch_num++) + ".patch";
      ofstream patch_file(patch_path);
      patch_file << patches[j];
      patch_file.close();
      clusters_patch_paths.back().push_back(patch_path);
      if (verbose >= 1) cout << "Wrote " << patch_path << endl;
    }
  }

  vector<future<string>> message_futures;
  vector<ClusteredCommit> commits;
  for (vector<string>& patch_paths: clusters_patch_paths) {
    // Skip clusters with no valid patches (all patches were empty)
    if (patch_paths.empty()) {
      if (verbose >= 1) cout << "Skipping cluster with no valid patches" << endl;
      continue;
    }
    string diff_context = "";
    ClusteredCommit commit{vector<string>(), "empty commit"};
    for (string path: patch_paths) {
      ifstream patch_file;
      patch_file.open(path);

      if (!patch_file.is_open()) {
        cerr << "Error opening file!" << endl;
        return 1;
      }

      string line;
      while (getline(patch_file, line)) {
        if (line[0] == '+') {
          diff_context += "Insertion: ";
        }
        else if (line[0] == '-') {
          diff_context += "Deletion: ";
        }
        diff_context += line + "\n";
      }
      patch_file.close();
      diff_context += "\n\n\n";
      commit.patch_paths.push_back(path);
    }

    message_futures.push_back(async_generate_commit_message(openai_api, diff_context));
    commits.push_back(commit);
  }

  openai_api.run_requests();

  for (size_t i = 0; i < commits.size(); i++) {
    commits[i].commit_message = message_futures[i].get();
  }

  // Serialize and write JSON to file
  json output = json::array();
  for (const ClusteredCommit& commit : commits) {
    output.push_back(commit.to_json());
  }

  ofstream commits_file("/tmp/patches/commits.json");
  commits_file << output.dump();
  commits_file.close();

  // Output visualization.json for interactive mode
  if (interactive && !umap_points.empty()) {
    json viz_output;

    // Points array
    json points_json = json::array();
    for (size_t i = 0; i < all_chunks.size(); i++) {
      string preview = combineContent(all_chunks[i]);
      if (preview.size() > 100) preview = preview.substr(0, 100) + "...";

      points_json.push_back({
        {"id", i},
        {"x", umap_points[i].x},
        {"y", umap_points[i].y},
        {"cluster_id", chunk_to_cluster[i]},
        {"filepath", all_chunks[i].filepath},
        {"preview", preview}
      });
    }
    viz_output["points"] = points_json;

    // Clusters array
    json clusters_json = json::array();
    for (size_t i = 0; i < commits.size(); i++) {
      clusters_json.push_back({
        {"id", i},
        {"message", commits[i].commit_message}
      });
    }
    viz_output["clusters"] = clusters_json;

    ofstream viz_file("/tmp/patches/visualization.json");
    viz_file << viz_output.dump(2);
    viz_file.close();

    if (verbose >= 1) cout << "Wrote visualization.json" << endl;
  }

  return 0;
}