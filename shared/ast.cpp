#include "ast.hpp"

using namespace std;
// Helper function to calculate the total character size of DiffLines
size_t calculateDiffLinesSize(const vector<DiffLine>& lines) {
  size_t totalSize = 0;
  for (const DiffLine& line : lines) {
      totalSize += line.content.length() + 1;
  }
  return totalSize;
}

// Line-based chunking for DiffChunk with character limit and line overlap
vector<DiffChunk> chunkByLines(const DiffChunk& inputChunk, size_t maxChars, size_t lineOverlap) {
  vector<DiffChunk> chunks;
  
  if (inputChunk.lines.empty()) {
      return chunks;
  }
  
  // If total chunk is smaller than max chunk size, return as single chunk
  size_t totalSize = calculateDiffLinesSize(inputChunk.lines);
  if (totalSize <= maxChars) {
      chunks.push_back(inputChunk);
      return chunks;
  }
  
  size_t startLineIdx = 0;
  while (startLineIdx < inputChunk.lines.size()) {
      DiffChunk currentChunk;
      currentChunk.filepath = inputChunk.filepath; // Preserve filepath
      size_t currentSize = 0;
      size_t currentLineIdx = startLineIdx;
      
      // Add lines until we exceed maxChars or run out of lines
      while (currentLineIdx < inputChunk.lines.size()) {
          const DiffLine& line = inputChunk.lines[currentLineIdx];
          size_t lineSize = line.content.length() + 1; // +1 for newline
          
          // Check if adding this line would exceed maxChars
          if (!currentChunk.lines.empty() && currentSize + lineSize > maxChars) {
              break;
          }
          
          currentChunk.lines.push_back(line);
          currentSize += lineSize;
          currentLineIdx++;
      }
      
      chunks.push_back(currentChunk);
      
      // Move start position with line overlap
      if (currentLineIdx >= inputChunk.lines.size()) {
          break;
      }
      
      // Calculate next start line with overlap
      size_t linesInChunk = currentLineIdx - startLineIdx;
      if (linesInChunk <= lineOverlap) {
          // If chunk has fewer lines than overlap, move by 1 line
          startLineIdx = startLineIdx + 1;
      } else {
          // Normal case: move by (lines in chunk - overlap)
          startLineIdx = startLineIdx + linesInChunk - lineOverlap;
      }
  }
  
  return chunks;
}

extern "C" {
TSLanguage* tree_sitter_python();
TSLanguage* tree_sitter_cpp();
TSLanguage* tree_sitter_java();
TSLanguage* tree_sitter_javascript();
// TSLanguage* tree_sitter_typescript();  // Temporarily disabled
TSLanguage* tree_sitter_go();
}



// New helper function that tracks processed lines
vector<DiffLine> extractLinesInRangeUnique(const vector<DiffLine>& diffLines, size_t startByte, size_t endByte, set<int>& processedLineNums) {
  vector<DiffLine> result;
  size_t currentByte = 0;
  
  for (const auto& line : diffLines) {
      size_t lineStart = currentByte;
      size_t lineEnd = currentByte + line.content.length() + 1; // +1 for newline
      
      // Check if this line overlaps with the target range AND hasn't been processed
      if (lineStart < endByte && lineEnd > startByte && processedLineNums.find(line.line_num) == processedLineNums.end()) {
          result.push_back(line);
          processedLineNums.insert(line.line_num);
      }
      
      currentByte = lineEnd;
      
      // Early exit if we've passed the end of the range
      if (currentByte >= endByte) {
          break;
      }
  }
  
  return result;
}

vector<DiffChunk> chunkDiff(const ts::Node& node, const DiffChunk& diffChunk, size_t maxChars) {
  vector<DiffChunk> newChunks;
  DiffChunk currentChunk;
  currentChunk.filepath = diffChunk.filepath;
  size_t currentChunkSize = 0;
  
  // Track which lines have been processed to avoid duplicates
  set<int> processedLineNums;
  
  for (size_t i = 0; i < node.getNumChildren(); i++) {
      ts::Node child = node.getChild(i);
      auto byteRange = child.getByteRange();
      
      vector<DiffLine> childLines = extractLinesInRangeUnique(diffChunk.lines, byteRange.start, byteRange.end, processedLineNums);
      size_t childSize = calculateDiffLinesSize(childLines);

      if (childSize > maxChars) {
          if (!currentChunk.lines.empty()) {
              newChunks.push_back(currentChunk);
              currentChunk = DiffChunk();
              currentChunk.filepath = diffChunk.filepath;
              currentChunkSize = 0;
          }
          auto childChunks = chunkDiff(child, diffChunk, maxChars);
          newChunks.insert(newChunks.end(), childChunks.begin(), childChunks.end());
      }
      else if (currentChunkSize + childSize > maxChars) {
          newChunks.push_back(currentChunk);
          currentChunk = DiffChunk();
          currentChunk.filepath = diffChunk.filepath;
          currentChunk.lines = childLines;
          currentChunkSize = childSize;
      }
      else {
          currentChunk.lines.insert(currentChunk.lines.end(), childLines.begin(), childLines.end());
          currentChunkSize += childSize;
      }
  }

  if (!currentChunk.lines.empty()) {
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
  } else if (extension == ".cpp") {
    return "cpp"; // Default fallback
  } else {
    return "text";
  }
}

