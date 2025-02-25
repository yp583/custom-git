#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <regex>
#include <algorithm>


#include <cpp-tree-sitter.h>
#include "ast.h"

using namespace std;


extern "C" {
TSLanguage* tree_sitter_python();
TSLanguage* tree_sitter_cpp();
}

vector<string> chunkNode(const ts::Node& node, const string& text, size_t maxChars) {
    vector<string> newChunks;
    string currentChunk;

    for (size_t i = 0; i < node.getNumChildren(); i++) {
        ts::Node child = node.getChild(i);
        auto byteRange = child.getByteRange();
        size_t childLength = byteRange.end - byteRange.start;

        if (childLength > maxChars) {
            if (!currentChunk.empty()) {
                newChunks.push_back(currentChunk);
                currentChunk.clear();
            }
            auto childChunks = chunkNode(child, text, maxChars);
            newChunks.insert(newChunks.end(), childChunks.begin(), childChunks.end());
        }
        else if (currentChunk.length() + childLength > maxChars) {
            newChunks.push_back(currentChunk);
            currentChunk = text.substr(byteRange.start, childLength);
        }
        else {
            currentChunk += text.substr(byteRange.start, childLength);
        }
    }

    if (!currentChunk.empty()) {
        newChunks.push_back(currentChunk);
    }

    return newChunks;
}

ts::Tree codeToTree(const string& code, const string& language) {
  TSLanguage* lang;
  if (language == "python") {
    lang = tree_sitter_python();
  } else if (language == "cpp") {
    lang = tree_sitter_cpp();
  }
  ts::Parser parser{lang};
  return parser.parseString(code);
}


// int main() {

//   string git_diff;
//   string line;
//   while (getline(cin, line))
//   {
//       git_diff += line + "\n";
//   }

//   size_t pos = 0;
//   while ((pos = git_diff.find("-", pos)) != string::npos) {
//       git_diff.replace(pos, 1, "");
//   }
//   string original_diff = git_diff;
//   regex re("(@@).+(@@)");
//   sregex_iterator regex_delim = sregex_iterator(git_diff.begin(), git_diff.end(), re);
//   sregex_iterator end = sregex_iterator();

//   vector<string> git_chunks;

//   for (sregex_iterator it = regex_delim; it != end; it++) {
//       smatch match = *it;
//       git_chunks.push_back(match.suffix());
//   }

//   ts::Tree tree = codeToTree(original_diff);
//   vector<string> chunks = chunkNode(tree.getRootNode(), original_diff);
//   for (int i = 0; i < chunks.size(); i++) {
//     cout << "Chunk " << i << ": " << chunks[i] << endl;
//   }
//   return 0;
// }


