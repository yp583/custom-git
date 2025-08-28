#include "hierarchal.h"
#include <queue>
#include <iostream>
#include <limits>

void print_dist_mat(vector<vector<float>>& dist_mat) {
  for (int i = 0; i < dist_mat.size(); i++) {
    for (int j = 0; j < dist_mat.size(); j++) {
      cout << dist_mat[i][j] << " ";
    }
    cout << endl;
  }
}
HierachicalClustering::HierachicalClustering() {}

void HierachicalClustering::cluster(vector<vector<float>> data, float distance_threshold) {
  // Initialize distance matrix
  vector<vector<float>> dist_mat(data.size(), vector<float>(data.size(), -1));
  
  for (int i = 0; i < data.size(); i++) {
    clusters.push_back({i});
  }

  // Calculate initial pairwise distances
  for (int i = 0; i < data.size(); i++) {
    for (int j = i + 1; j < data.size(); j++) {
      dist_mat[i][j] = 1 - cos_sim(data[i], data[j]);
    }
  }

  while (clusters.size() > 1) {
    float min_dist = numeric_limits<float>::infinity();
    int min_i = -1;
    int min_j = -1;
    
    // Find closest pair of clusters
    for (int i = 0; i < clusters.size(); i++) {
      for (int j = i + 1; j < clusters.size(); j++) {
        float cluster_dist = numeric_limits<float>::infinity();
        for (int pi : clusters[i]) {
          for (int pj : clusters[j]) {
            if (dist_mat[pi][pj] < cluster_dist) {
              cluster_dist = dist_mat[pi][pj];
            }
          }
        }
        if (cluster_dist < min_dist) {
          min_dist = cluster_dist;
          min_i = i;
          min_j = j;
        }
      }
    }

    if (min_dist > distance_threshold) {
      break;
    }

    for (int point : clusters[min_j]) {
      clusters[min_i].push_back(point);
    }
    clusters.erase(clusters.begin() + min_j);
  }
}

HierachicalClustering::~HierachicalClustering() {}

vector<vector<int>> HierachicalClustering::get_clusters() {
  return clusters;
}

// int main() {
//   vector<vector<float>> data = {
//     {0.267261, 0.534522, 0.801784},  // [1,2,3] normalized
//     {0.455842, 0.569802, 0.683763},  // [4,5,6] normalized  
//     {0.487494, 0.650164, 0.582647},  // [7,8,9] normalized
//     {0.577350, 0.577350, 0.577350}   // [1,1,1] normalized
//   };
//   HierachicalClustering hc;
//   hc.cluster(data, 0.02);  // Stop clustering when distance > 0.3
//   return 0;
// }
