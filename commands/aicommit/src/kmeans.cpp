#include "kmeans.h"
#include <algorithm>
#include <random>
#include <cmath>



KMeans::KMeans(int k, int max_iter, int dim) : k(k), max_iter(max_iter) {
  centroids = vector<vector<float>>(k, vector<float>(dim));
}

void KMeans::fit(vector<vector<float>> data) {

    random_device rnd_device;
    mt19937 mersenne_engine {rnd_device()}; 
    uniform_int_distribution<float> dist {0, 1};
    
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

void KMeans::predict(vector<float> data) {
    this->centroids = data;
}