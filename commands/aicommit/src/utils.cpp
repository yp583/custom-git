#include "utils.h"

float cos_sim(vector<float> a, vector<float> b) {
  float dot = 0.0;
  for (int i = 0; i < a.size(); i++) {
    dot += a[i] * b[i];
  }
  //assuming vectors are normalized
  return dot; 
}