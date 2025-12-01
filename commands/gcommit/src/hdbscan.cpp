#include "hdbscan.hpp"
#include "Hdbscan/hdbscan.hpp"
#include <unordered_map>

HDBSCANClustering::HDBSCANClustering(int min_cluster_size, int min_pts)
    : min_cluster_size(min_cluster_size), min_pts(min_pts) {}

HDBSCANClustering::~HDBSCANClustering() {}

void HDBSCANClustering::fit(const vector<vector<float>>& data) {
  clusters.clear();
  labels.clear();

  if (data.empty()) {
    return;
  }

  Hdbscan hdbscan("");

  hdbscan.dataset.resize(data.size());
  for (size_t i = 0; i < data.size(); i++) {
    hdbscan.dataset[i].resize(data[i].size());
    for (size_t j = 0; j < data[i].size(); j++) {
      hdbscan.dataset[i][j] = static_cast<double>(data[i][j]);
    }
  }

  hdbscan.execute(min_pts, min_cluster_size, "Euclidean");

  labels = hdbscan.normalizedLabels_;

  std::unordered_map<int, vector<int>> cluster_map;
  vector<int> noise_points;

  for (size_t i = 0; i < labels.size(); i++) {
    if (labels[i] == -1) {
      noise_points.push_back(static_cast<int>(i));
    } else {
      cluster_map[labels[i]].push_back(static_cast<int>(i));
    }
  }

  for (auto& kv : cluster_map) {
    clusters.push_back(kv.second);
  }

  for (int noise_idx : noise_points) {
    clusters.push_back({noise_idx});
  }
}

vector<vector<int>> HDBSCANClustering::get_clusters() {
  return clusters;
}

vector<int> HDBSCANClustering::get_labels() {
  return labels;
}
