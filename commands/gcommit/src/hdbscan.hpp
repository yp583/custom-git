#pragma once
#include <vector>

using namespace std;

class HDBSCANClustering {
private:
  vector<vector<int>> clusters;
  vector<int> labels;
  int min_cluster_size;
  int min_pts;

public:
  HDBSCANClustering(int min_cluster_size = 2, int min_pts = 2);

  // Fit HDBSCAN on embedding data
  // Data: vector of embedding vectors (each embedding is a vector of floats)
  void fit(const vector<vector<float>>& data);

  // Get clusters as vector of point indices (same format as HierarchicalClustering)
  vector<vector<int>> get_clusters();

  // Get raw labels for each point (-1 = noise)
  vector<int> get_labels();

  ~HDBSCANClustering();
};
