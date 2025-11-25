#include <gtest/gtest.h>
#include "hierarchal.hpp"

class HierarchicalClusteringTest : public ::testing::Test {
protected:
    void SetUp() override {
        hc = std::make_unique<HierachicalClustering>();
    }

    std::unique_ptr<HierachicalClustering> hc;
};

TEST_F(HierarchicalClusteringTest, EmptyInput) {
    std::vector<std::vector<float>> embeddings;

    hc->cluster(embeddings, 0.5f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    EXPECT_EQ(clusters.size(), 0);
}

TEST_F(HierarchicalClusteringTest, SingleElement) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f}
    };

    hc->cluster(embeddings, 0.5f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    ASSERT_EQ(clusters.size(), 1);
    EXPECT_EQ(clusters[0].size(), 1);
    EXPECT_EQ(clusters[0][0], 0);
}

TEST_F(HierarchicalClusteringTest, TwoIdenticalVectorsMerge) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f}
    };

    hc->cluster(embeddings, 0.5f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    // Identical vectors should merge into one cluster
    ASSERT_EQ(clusters.size(), 1);
    EXPECT_EQ(clusters[0].size(), 2);
}

TEST_F(HierarchicalClusteringTest, TwoOrthogonalVectorsStaySeparate) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };

    // Distance = 1 - cos_sim = 1 - 0 = 1, which is > 0.5 threshold
    hc->cluster(embeddings, 0.5f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    EXPECT_EQ(clusters.size(), 2);
}

TEST_F(HierarchicalClusteringTest, ThreeVectorsTwoSimilar) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f},
        {0.95f, 0.05f, 0.0f},  // Very similar to first
        {0.0f, 1.0f, 0.0f}     // Orthogonal
    };

    hc->cluster(embeddings, 0.3f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    // First two should cluster, third stays separate
    EXPECT_EQ(clusters.size(), 2);
}

TEST_F(HierarchicalClusteringTest, HighThresholdMergesAll) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    // Very high threshold should merge everything
    hc->cluster(embeddings, 2.0f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    ASSERT_EQ(clusters.size(), 1);
    EXPECT_EQ(clusters[0].size(), 3);
}

TEST_F(HierarchicalClusteringTest, ZeroThresholdKeepsSeparate) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f},
        {0.99f, 0.01f, 0.0f}
    };

    // Zero threshold - only identical vectors merge
    hc->cluster(embeddings, 0.0f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    EXPECT_EQ(clusters.size(), 2);
}

TEST_F(HierarchicalClusteringTest, ClusterIndicesAreCorrect) {
    std::vector<std::vector<float>> embeddings = {
        {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };

    hc->cluster(embeddings, 0.5f);
    std::vector<std::vector<int>> clusters = hc->get_clusters();

    // Check all indices are present
    std::set<int> all_indices;
    for (const auto& cluster : clusters) {
        for (int idx : cluster) {
            all_indices.insert(idx);
        }
    }

    EXPECT_EQ(all_indices.size(), 3);
    EXPECT_TRUE(all_indices.count(0));
    EXPECT_TRUE(all_indices.count(1));
    EXPECT_TRUE(all_indices.count(2));
}
