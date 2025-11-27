#ifndef UMAP_HPP
#define UMAP_HPP

#include <vector>
#include <algorithm>
#include "umappp/umappp.hpp"
#include "knncolle/knncolle.hpp"

using namespace std;

struct UmapPoint {
  double x;
  double y;
};

// Compute UMAP dimensionality reduction on embeddings
// Input: vector of embedding vectors (each is 1536D or similar)
// Output: vector of 2D points
inline vector<UmapPoint> compute_umap(const vector<vector<float>>& embeddings, int num_neighbors = 15, int num_epochs = 200) {
  if (embeddings.empty() || embeddings[0].empty()) {
    return {};
  }

  size_t nobs = embeddings.size();
  size_t ndim = embeddings[0].size();

  // Convert embeddings to column-major format for umappp
  vector<double> data(ndim * nobs);
  for (size_t i = 0; i < nobs; i++) {
    for (size_t j = 0; j < ndim && j < embeddings[i].size(); j++) {
      data[j + i * ndim] = static_cast<double>(embeddings[i][j]);
    }
  }

  // Set up neighbor search with Euclidean distance
  knncolle::VptreeBuilder<knncolle::EuclideanDistance, int, double, double> vp_builder;

  // Initialize UMAP with 2D output
  size_t out_dim = 2;
  vector<double> umap_coords(nobs * out_dim);

  umappp::Options opt;
  opt.num_neighbors = min(static_cast<int>(nobs) - 1, num_neighbors);
  opt.num_epochs = num_epochs;

  auto status = umappp::initialize(
    ndim, nobs, data.data(), vp_builder, out_dim, umap_coords.data(), opt
  );
  status.run();

  // Convert to UmapPoint vector
  vector<UmapPoint> points(nobs);
  for (size_t i = 0; i < nobs; i++) {
    points[i].x = umap_coords[i * 2];
    points[i].y = umap_coords[i * 2 + 1];
  }

  return points;
}

#endif // UMAP_HPP
