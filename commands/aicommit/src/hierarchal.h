#include <vector>
#include "utils.h"

using namespace std;

class HierachicalClustering {
private:
  vector<vector<int>> clusters;
public:
    HierachicalClustering();
    void cluster(vector<vector<float>> data, float distance_threshold = 0.5);
    vector<vector<int>> get_clusters();
    ~HierachicalClustering();
};
