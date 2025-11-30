#pragma once
#include <vector>

using namespace std;

// MST edge for hierarchical clustering
struct MSTEdge {
  int a;
  int b;
  double distance;
};

class HDBSCANClustering {
private:
  vector<vector<int>> clusters;
  vector<int> labels;
  vector<MSTEdge> mst_edges;  // sorted by distance ascending
  int num_points;
  int min_cluster_size;
  int min_pts;

public:
  HDBSCANClustering(int min_cluster_size = 2, int min_pts = 2);
  void fit(const vector<vector<float>>& data);
  vector<vector<int>> get_clusters();
  vector<int> get_labels();

  // Get MST edges (sorted by distance)
  const vector<MSTEdge>& get_mst_edges() const { return mst_edges; }

  // Get clusters by cutting MST at epsilon threshold (O(N) - instant)
  vector<int> get_clusters_at_epsilon(double epsilon) const;

  ~HDBSCANClustering();
};
