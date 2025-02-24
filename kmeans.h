#include <vector>
using namespace std;

class KMeans {
private:
    int k;
    int max_iter;
    vector<vector<float>> centroids;
    int dim;
public:
    KMeans(int k, int max_iter, int dim);

    void fit(vector<vector<float>> data);
    void predict(vector<float> data);
    ~KMeans();
};
