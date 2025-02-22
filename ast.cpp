#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <cpp-tree-sitter.h>

using namespace std;


extern "C" {
TSLanguage* tree_sitter_python();
}

vector<string> chunkNode(const ts::Node& node, const string& text, size_t maxChars = 1500) {
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


int main() {
  // Create a language and parser.
  ts::Language language = tree_sitter_python();
  ts::Parser parser{language};

  // Parse the provided string into a syntax tree.
//   string sourcecode = "[1, null]";

  string sourcecode;
  string line;
  while (getline(cin, line))
  {
      sourcecode += line + "\n";
  }
  
  ts::Tree tree = parser.parseString(sourcecode);

  // Get the root node of the syntax tree. 
  ts::Node root = tree.getRootNode();

  // Get some child nodes.
  ts::Node array = root.getNamedChild(0);
  ts::Node number = array.getNamedChild(0);

  // Check that the nodes have the expected types.

  // Check that the nodes have the expected child counts.

  // Print the syntax tree as an S-expression.
//   auto treestring = root.getSExpr();
//   printf("Syntax tree: %s\n", treestring.get());

  vector<string> chunks = chunkNode(root, sourcecode);

  for (int i = 0; i < chunks.size(); i++) {
    cout << "Chunk " << i << ": " << chunks[i] << endl;
  }

  return 0;
}