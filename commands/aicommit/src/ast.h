#include <string>
#include "tree_sitter/api.h"
#include <vector>
using namespace std;
// Function declarations
vector<string> chunkNode(const ts::Node& node, const string& text, size_t maxChars = 1500);
ts::Tree codeToTree(const string& code, const string& language);
string detectLanguageFromPath(const string& filepath);
vector<string> chunkByCharacters(const string& text, size_t maxChars = 1000, size_t overlap = 100);
bool isTextFile(const string& filepath); 