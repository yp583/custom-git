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

// Character-based chunking for non-code files
vector<string> chunkByCharacters(const string& text, size_t maxChars, size_t overlap) {
    vector<string> chunks;
    
    if (text.empty()) {
        return chunks;
    }
    
    // If text is smaller than max chunk size, return as single chunk
    if (text.length() <= maxChars) {
        chunks.push_back(text);
        return chunks;
    }
    
    size_t start = 0;
    while (start < text.length()) {
        size_t chunkSize = min(maxChars, text.length() - start);
        string chunk = text.substr(start, chunkSize);
        
        // Try to break at natural boundaries (newlines, spaces) when possible
        if (start + chunkSize < text.length() && chunkSize == maxChars) {
            // Look for a good breaking point within the last 10% of the chunk
            size_t searchStart = static_cast<size_t>(chunkSize * 0.9);
            size_t newlinePos = chunk.find_last_of('\n', searchStart);
            size_t spacePos = chunk.find_last_of(' ', searchStart);
            
            size_t breakPos = string::npos;
            if (newlinePos != string::npos) {
                breakPos = newlinePos + 1; // Include the newline
            } else if (spacePos != string::npos) {
                breakPos = spacePos + 1; // Include the space
            }
            
            if (breakPos != string::npos && breakPos > chunkSize / 2) {
                chunk = chunk.substr(0, breakPos);
                chunkSize = breakPos;
            }
        }
        
        chunks.push_back(chunk);
        
        // Move start position with overlap
        if (start + chunkSize >= text.length()) {
            break;
        }
        start += chunkSize - overlap;
    }
    
    return chunks;
}

// Check if file extension indicates a text/non-code file
bool isTextFile(const string& filepath) {
    size_t lastDot = filepath.find_last_of(".");
    if (lastDot == string::npos) {
        return false;
    }
    
    string extension = filepath.substr(lastDot);
    return (extension == ".txt" || extension == ".md" || extension == ".json" || 
            extension == ".xml" || extension == ".yaml" || extension == ".yml" ||
            extension == ".csv" || extension == ".log" || extension == ".conf" ||
            extension == ".ini" || extension == ".toml" || extension == ".rst");
}

extern "C" {
TSLanguage* tree_sitter_python();
TSLanguage* tree_sitter_cpp();
TSLanguage* tree_sitter_java();
TSLanguage* tree_sitter_javascript();
// TSLanguage* tree_sitter_typescript();  // Temporarily disabled
TSLanguage* tree_sitter_go();
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
  } else if (language == "java") {
    lang = tree_sitter_java();
  } else if (language == "javascript") {
    lang = tree_sitter_javascript();
  } else if (language == "typescript") {
    // TypeScript support temporarily disabled, fallback to JavaScript
    lang = tree_sitter_javascript();
  } else if (language == "go") {
    lang = tree_sitter_go();
  } else {
    // Default to C++ for unknown languages
    lang = tree_sitter_cpp();
  }
  ts::Parser parser{lang};
  return parser.parseString(code);
}

string detectLanguageFromPath(const string& filepath) {
  // Find the last dot for file extension
  size_t lastDot = filepath.find_last_of(".");
  if (lastDot == string::npos) {
    return "cpp"; // Default fallback
  }
  
  string extension = filepath.substr(lastDot);
  
  // Map file extensions to languages
  if (extension == ".py") {
    return "python";
  } else if (extension == ".cpp" || extension == ".c" || extension == ".h" || extension == ".hpp") {
    return "cpp";
  } else if (extension == ".java") {
    return "java";
  } else if (extension == ".js" || extension == ".jsx") {
    return "javascript";
  } else if (extension == ".ts" || extension == ".tsx") {
    return "typescript";
  } else if (extension == ".go") {
    return "go";
  } else {
    return "cpp"; // Default fallback
  }
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


