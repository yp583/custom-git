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

// Helper to calculate line offsets based on DiffLine modes
pair<int, int> calculateLineOffsets(const vector<DiffLine>& lines, size_t startIdx, size_t endIdx) {
    int old_offset = 0, new_offset = 0;
    for (size_t i = startIdx; i < endIdx && i < lines.size(); i++) {
        switch (lines[i].mode) {
            case EQ:        old_offset++; new_offset++; break;
            case DELETION:  old_offset++; break;
            case INSERTION: new_offset++; break;
        }
    }
    return {old_offset, new_offset};
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
  int cumulative_old_offset = 0;
  int cumulative_new_offset = 0;

  while (startLineIdx < inputChunk.lines.size()) {
      DiffChunk currentChunk;
      currentChunk.filepath = inputChunk.filepath;
      currentChunk.old_start = inputChunk.old_start + cumulative_old_offset;
      currentChunk.new_start = inputChunk.new_start + cumulative_new_offset;

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
      size_t nextStartIdx;
      if (linesInChunk <= lineOverlap) {
          nextStartIdx = startLineIdx + 1;
      } else {
          nextStartIdx = startLineIdx + linesInChunk - lineOverlap;
      }

      // Update cumulative offsets for the lines we're moving past
      auto [old_off, new_off] = calculateLineOffsets(inputChunk.lines, startLineIdx, nextStartIdx);
      cumulative_old_offset += old_off;
      cumulative_new_offset += new_off;

      startLineIdx = nextStartIdx;
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

// Helper to find the first line index that matches a DiffLine (by line_num)
int findLineIndex(const vector<DiffLine>& allLines, int target_line_num) {
    for (size_t i = 0; i < allLines.size(); i++) {
        if (allLines[i].line_num == target_line_num) {
            return i;
        }
    }
    return 0;
}

// Helper to fill in gap lines between extracted lines
// This ensures empty lines and other lines not covered by AST nodes are included
// Also adds leading and trailing context lines needed for valid patches
DiffChunk fillGapLines(const DiffChunk& chunk, const vector<DiffLine>& allLines, int contextLines) {
    if (chunk.lines.empty()) return chunk;

    DiffChunk result = chunk;
    result.lines.clear();

    int minLineNum = chunk.lines.front().line_num;
    int maxLineNum = chunk.lines.back().line_num;

    // Find index of first line (minLineNum) in allLines
    size_t firstLineIdx = 0;
    for (size_t i = 0; i < allLines.size(); i++) {
        if (allLines[i].line_num == minLineNum) {
            firstLineIdx = i;
            break;
        }
    }

    // Find index of last line (maxLineNum) in allLines
    size_t lastLineIdx = 0;
    for (size_t i = 0; i < allLines.size(); i++) {
        if (allLines[i].line_num == maxLineNum) {
            lastLineIdx = i;
            break;
        }
    }

    // Extend range to include leading context (only EQ lines)
    int leadingAdded = 0;
    for (int i = static_cast<int>(firstLineIdx) - 1; i >= 0 && leadingAdded < contextLines; i--) {
        if (allLines[i].mode == EQ) {
            minLineNum = allLines[i].line_num;
            leadingAdded++;
        } else {
            break;  // Stop if we hit a non-context line
        }
    }

    // Extend range to include trailing context (only EQ lines)
    int trailingAdded = 0;
    for (size_t i = lastLineIdx + 1; i < allLines.size() && trailingAdded < contextLines; i++) {
        if (allLines[i].mode == EQ) {
            maxLineNum = allLines[i].line_num;
            trailingAdded++;
        } else {
            break;  // Stop if we hit a non-context line
        }
    }

    // Include all lines from allLines that fall within the extended range
    for (const auto& line : allLines) {
        if (line.line_num >= minLineNum && line.line_num <= maxLineNum) {
            result.lines.push_back(line);
        }
    }

    // Adjust start positions for leading context added
    result.old_start = chunk.old_start - leadingAdded;
    result.new_start = chunk.new_start - leadingAdded;

    return result;
}

// Merge chunks that would have overlapping context (< 2*contextLines apart)
vector<DiffChunk> mergeOverlappingChunks(vector<DiffChunk> chunks, int contextLines) {
    if (chunks.size() <= 1) return chunks;

    // Sort chunks by filepath, then by old_start
    sort(chunks.begin(), chunks.end(), [](const DiffChunk& a, const DiffChunk& b) {
        if (a.filepath != b.filepath) return a.filepath < b.filepath;
        return a.old_start < b.old_start;
    });

    vector<DiffChunk> merged;
    DiffChunk current = chunks[0];

    for (size_t i = 1; i < chunks.size(); i++) {
        const DiffChunk& next = chunks[i];

        // Different file - no merge possible
        if (current.filepath != next.filepath) {
            merged.push_back(current);
            current = next;
            continue;
        }

        // Check if chunks would overlap with context
        // Use old_start to determine actual file position gap
        int currentEndLine = current.old_start + static_cast<int>(current.lines.size());
        int nextStartLine = next.old_start;

        // If gap between chunks (in actual file lines) is less than 2*contextLines, they'd overlap
        // But don't merge if they're from very different positions (different hunks)
        int gap = nextStartLine - currentEndLine;
        if (gap >= 0 && gap <= 2 * contextLines) {
            // Merge: extend current to include next's lines
            for (const auto& line : next.lines) {
                // Avoid duplicates
                bool duplicate = false;
                for (const auto& existing : current.lines) {
                    if (existing.line_num == line.line_num) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    current.lines.push_back(line);
                }
            }
            // Sort lines by line_num after merge
            sort(current.lines.begin(), current.lines.end(),
                 [](const DiffLine& a, const DiffLine& b) { return a.line_num < b.line_num; });
        } else {
            // No overlap - push current and start new
            merged.push_back(current);
            current = next;
        }
    }

    // Don't forget the last chunk
    merged.push_back(current);

    return merged;
}

vector<DiffChunk> chunkDiff(const ts::Node& node, const DiffChunk& diffChunk, size_t maxChars) {
  vector<DiffChunk> newChunks;
  DiffChunk currentChunk;
  currentChunk.filepath = diffChunk.filepath;
  currentChunk.old_start = diffChunk.old_start;
  currentChunk.new_start = diffChunk.new_start;
  size_t currentChunkSize = 0;
  bool currentChunkStartSet = false;

  // Track which lines have been processed to avoid duplicates
  set<int> processedLineNums;

  for (size_t i = 0; i < node.getNumChildren(); i++) {
      ts::Node child = node.getChild(i);
      auto byteRange = child.getByteRange();

      vector<DiffLine> childLines = extractLinesInRangeUnique(diffChunk.lines, byteRange.start, byteRange.end, processedLineNums);
      size_t childSize = calculateDiffLinesSize(childLines);

      if (childSize > maxChars) {
          if (!currentChunk.lines.empty()) {
              newChunks.push_back(fillGapLines(currentChunk, diffChunk.lines, 3));
              currentChunk = DiffChunk();
              currentChunk.filepath = diffChunk.filepath;
              currentChunkSize = 0;
              currentChunkStartSet = false;
          }
          auto childChunks = chunkDiff(child, diffChunk, maxChars);
          newChunks.insert(newChunks.end(), childChunks.begin(), childChunks.end());
      }
      else if (currentChunkSize + childSize > maxChars) {
          newChunks.push_back(fillGapLines(currentChunk, diffChunk.lines, 3));
          currentChunk = DiffChunk();
          currentChunk.filepath = diffChunk.filepath;
          currentChunk.lines = childLines;
          currentChunkSize = childSize;

          // Calculate start position for new chunk
          if (!childLines.empty()) {
              int firstLineIdx = findLineIndex(diffChunk.lines, childLines[0].line_num);
              auto [old_off, new_off] = calculateLineOffsets(diffChunk.lines, 0, firstLineIdx);
              currentChunk.old_start = diffChunk.old_start + old_off;
              currentChunk.new_start = diffChunk.new_start + new_off;
          }
          currentChunkStartSet = true;
      }
      else {
          // Set start position when adding first lines to chunk
          if (!currentChunkStartSet && !childLines.empty()) {
              int firstLineIdx = findLineIndex(diffChunk.lines, childLines[0].line_num);
              auto [old_off, new_off] = calculateLineOffsets(diffChunk.lines, 0, firstLineIdx);
              currentChunk.old_start = diffChunk.old_start + old_off;
              currentChunk.new_start = diffChunk.new_start + new_off;
              currentChunkStartSet = true;
          }
          currentChunk.lines.insert(currentChunk.lines.end(), childLines.begin(), childLines.end());
          currentChunkSize += childSize;
      }
  }

  if (!currentChunk.lines.empty()) {
      newChunks.push_back(fillGapLines(currentChunk, diffChunk.lines, 3));
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

