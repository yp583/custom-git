#include "kmeans.hpp"




KMeans::KMeans(int k, int max_iter, int dim) : k(k), max_iter(max_iter) {
  centroids = vector<vector<float>>(k, vector<float>(dim));
}

void KMeans::fit(vector<vector<float>> data) {

    random_device rnd_device;
    mt19937 mersenne_engine {rnd_device()}; 
    uniform_real_distribution<float> dist{0.0f, 1.0f};
    
    auto gen = [&](){
                   return dist(mersenne_engine);
               };

    for (int i = 0; i < k; i++) {
        generate(centroids[i].begin(), centroids[i].end(), gen);
    }
    int curr_iter = 0;
    while (curr_iter < max_iter) {
        vector<vector<float>> new_centroids(k, vector<float>(dim));
        for (int i = 0; i < k; i++) {
            generate(new_centroids[i].begin(), new_centroids[i].end(), gen);
        }
        curr_iter++;
    }
}

int KMeans::predict(vector<float> data) {
    // Find closest centroid
    int closest_cluster = 0;
    float min_distance = INFINITY;
    
    for (int i = 0; i < k; i++) {
        float distance = 0;
        for (int j = 0; j < data.size(); j++) {
            distance += (data[j] - centroids[i][j]) * (data[j] - centroids[i][j]);
        }
        if (distance < min_distance) {
            min_distance = distance;
            closest_cluster = i;
        }
    }
    return closest_cluster;
}