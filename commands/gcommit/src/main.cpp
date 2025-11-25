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

  vector<DiffChunk> all_chunks;

  for (const DiffFile& file : dr.getFiles()) {
    string language = detectLanguageFromPath(file.filepath);
    DiffChunk file_chunk = getDiffContent(file, {});  // All lines - no filtering

    vector<DiffChunk> file_chunks;

    if (language != "text") { //use ast for non text files
      string file_content = combineContent(file_chunk);
      ts::Tree tree = codeToTree(file_content, language);
      file_chunks = chunkDiff(tree.getRootNode(), file_chunk);
    }
    else {
      file_chunks = chunkByLines(file_chunk);
    }

    all_chunks.insert(all_chunks.end(), file_chunks.begin(), file_chunks.end());
  }

  // Merge overlapping chunks to maintain non-overlap invariant
  all_chunks = mergeOverlappingChunks(all_chunks, 3);

  // // Generate embeddings (progress to stderr so it doesn't interfere with JSON output)
  // cerr << "Embedding " << all_chunks.size() << " chunks" << endl;

  AsyncHTTPSConnection conn;
  AsyncOpenAIAPI openai_api(conn, api_key);
  vector<future<HTTPSResponse>> embedding_resp_futures;

  cout << "Adding Embedding requests to the queue" << endl;

  for (size_t i = 0; i < all_chunks.size(); i++){
    string content = combineContent(all_chunks[i]);

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

  vector<DiffChunk> all_cluster_chunks;
  vector<size_t> cluster_end_idx;
  

  for (size_t i = 0; i < clusters.size(); i++) {
    const vector<int>& cluster = clusters[i];

    cout << "Cluster " << (i + 1) << ":" << endl;

    // Collect chunks for this cluster
    for (int idx: cluster) {
      all_cluster_chunks.push_back(all_chunks[idx]);
    }
    size_t prev_end = cluster_end_idx.empty() ? 0 : cluster_end_idx.back();
    cluster_end_idx.push_back(prev_end + cluster.size());
  }

  vector<string> patches = createPatches(all_cluster_chunks);

  // Write patches to cluster folders
  for (size_t i = 0; i < cluster_end_idx.size(); i++) {
    string cluster_dir = "/tmp/patches/cluster_" + to_string(i);
    filesystem::create_directories(cluster_dir);

    size_t start_idx = (i == 0) ? 0 : cluster_end_idx[i - 1];
    size_t end_idx = cluster_end_idx[i];

    for (size_t j = start_idx; j < end_idx && j < patches.size(); j++) {
      string patch_path = cluster_dir + "/patch_" + to_string(j - start_idx) + ".patch";
      ofstream patch_file(patch_path);
      patch_file << patches[j];
      patch_file.close();
      cout << "Wrote " << patch_path << endl;
    }
  }
}