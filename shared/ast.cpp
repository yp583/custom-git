#include "ast.hpp"

using namespace std;
size_t calculateDiffLinesSize(const vector<DiffLine> &lines) {
  size_t totalSize = 0;
  for (const DiffLine &line : lines) {
    totalSize += line.content.length() + 1;
  }
  return totalSize;
}

int calculateLineOffset(const vector<DiffLine> &lines, size_t startIdx, size_t endIdx) {
  int offset = 0;
  for (size_t i = startIdx; i < endIdx && i < lines.size(); i++) {
    switch (lines[i].mode) {
    case EQ:
    case DELETION:
      offset++;
      break;
    case INSERTION:
    case NO_NEWLINE:
      break;
    }
  }
  return offset;
}

vector<DiffChunk> chunkByLines(const DiffChunk &inputChunk, size_t maxChars) {
  vector<DiffChunk> chunks;

  if (inputChunk.lines.empty()) {
    return chunks;
  }

  size_t totalSize = calculateDiffLinesSize(inputChunk.lines);
  if (totalSize <= maxChars) {
    chunks.push_back(inputChunk);
    return chunks;
  }

  size_t startLineIdx = 0;
  int cumulative_offset = 0;

  while (startLineIdx < inputChunk.lines.size()) {
    DiffChunk currentChunk;
    currentChunk.filepath = inputChunk.filepath;
    currentChunk.start = inputChunk.start + cumulative_offset;

    size_t currentSize = 0;
    size_t currentLineIdx = startLineIdx;

    while (currentLineIdx < inputChunk.lines.size()) {
      const DiffLine &line = inputChunk.lines[currentLineIdx];
      size_t lineSize = line.content.length() + 1;

      if (!currentChunk.lines.empty() && currentSize + lineSize > maxChars) {
        break;
      }

      currentChunk.lines.push_back(line);
      currentSize += lineSize;
      currentLineIdx++;
    }

    chunks.push_back(currentChunk);

    if (currentLineIdx >= inputChunk.lines.size()) {
      break;
    }

    cumulative_offset += calculateLineOffset(inputChunk.lines, startLineIdx, currentLineIdx);
    startLineIdx = currentLineIdx;
  }

  return chunks;
}

extern "C" {
TSLanguage *tree_sitter_python();
TSLanguage *tree_sitter_cpp();
TSLanguage *tree_sitter_java();
TSLanguage *tree_sitter_javascript();
TSLanguage *tree_sitter_go();
}

vector<DiffLine> extractLinesInRangeUnique(const vector<DiffLine> &diffLines,
                                           size_t startByte, size_t endByte,
                                           set<int> &processedLineNums) {
  vector<DiffLine> result;
  size_t currentByte = 0;

  for (const auto &line : diffLines) {
    size_t lineStart = currentByte;
    size_t lineEnd = currentByte + line.content.length() + 1;

    if (lineStart < endByte && lineEnd > startByte &&
        processedLineNums.find(line.line_num) == processedLineNums.end()) {
      result.push_back(line);
      processedLineNums.insert(line.line_num);
    }

    currentByte = lineEnd;

    if (currentByte >= endByte) {
      break;
    }
  }

  return result;
}

int findLineIndex(const vector<DiffLine> &allLines, int target_line_num) {
  for (size_t i = 0; i < allLines.size(); i++) {
    if (allLines[i].line_num == target_line_num) {
      return i;
    }
  }
  return 0;
}

DiffChunk fillGapLines(const DiffChunk &chunk, const vector<DiffLine> &allLines) {
  if (chunk.lines.empty())
    return chunk;

  DiffChunk result = chunk;
  result.lines.clear();

  int minLineNum = chunk.lines.front().line_num;
  int maxLineNum = chunk.lines.back().line_num;

  for (const auto &line : allLines) {
    if (line.line_num >= minLineNum && line.line_num <= maxLineNum) {
      result.lines.push_back(line);
    }
  }

  return result;
}

vector<DiffChunk> chunkDiff(const ts::Node &node, const DiffChunk &diffChunk,
                            size_t maxChars) {
  vector<DiffChunk> newChunks;
  DiffChunk currentChunk;
  currentChunk.filepath = diffChunk.filepath;
  currentChunk.start = diffChunk.start;
  size_t currentChunkSize = 0;
  bool currentChunkStartSet = false;

  set<int> processedLineNums;

  for (size_t i = 0; i < node.getNumChildren(); i++) {
    ts::Node child = node.getChild(i);
    auto byteRange = child.getByteRange();

    vector<DiffLine> childLines = extractLinesInRangeUnique(
        diffChunk.lines, byteRange.start, byteRange.end, processedLineNums);
    size_t childSize = calculateDiffLinesSize(childLines);

    if (childSize > maxChars) {
      if (!currentChunk.lines.empty()) {
        newChunks.push_back(fillGapLines(currentChunk, diffChunk.lines));
        currentChunk = DiffChunk();
        currentChunk.filepath = diffChunk.filepath;
        currentChunkSize = 0;
        currentChunkStartSet = false;
      }
      auto childChunks = chunkDiff(child, diffChunk, maxChars);
      newChunks.insert(newChunks.end(), childChunks.begin(), childChunks.end());
    } else if (currentChunkSize + childSize > maxChars) {
      newChunks.push_back(fillGapLines(currentChunk, diffChunk.lines));
      currentChunk = DiffChunk();
      currentChunk.filepath = diffChunk.filepath;
      currentChunk.lines = childLines;
      currentChunkSize = childSize;

      if (!childLines.empty()) {
        int firstLineIdx = findLineIndex(diffChunk.lines, childLines[0].line_num);
        currentChunk.start = diffChunk.start + calculateLineOffset(diffChunk.lines, 0, firstLineIdx);
      }
      currentChunkStartSet = true;
    } else {
      if (!currentChunkStartSet && !childLines.empty()) {
        int firstLineIdx = findLineIndex(diffChunk.lines, childLines[0].line_num);
        currentChunk.start = diffChunk.start + calculateLineOffset(diffChunk.lines, 0, firstLineIdx);
        currentChunkStartSet = true;
      }
      currentChunk.lines.insert(currentChunk.lines.end(), childLines.begin(),
                                childLines.end());
      currentChunkSize += childSize;
    }
  }

  if (!currentChunk.lines.empty()) {
    newChunks.push_back(fillGapLines(currentChunk, diffChunk.lines));
  }

  return newChunks;
}

ts::Tree codeToTree(const string &code, const string &language) {
  TSLanguage *lang;
  if (language == "python") {
    lang = tree_sitter_python();
  } else if (language == "cpp") {
    lang = tree_sitter_cpp();
  } else if (language == "java") {
    lang = tree_sitter_java();
  } else if (language == "javascript") {
    lang = tree_sitter_javascript();
  } else if (language == "typescript") {
    lang = tree_sitter_javascript();
  } else if (language == "go") {
    lang = tree_sitter_go();
  } else {
    lang = tree_sitter_cpp();
  }
  ts::Parser parser{lang};
  return parser.parseString(code);
}

string detectLanguageFromPath(const string &filepath) {
  size_t lastDot = filepath.find_last_of(".");
  if (lastDot == string::npos) {
    return "cpp";
  }

  string extension = filepath.substr(lastDot);

  if (extension == ".py") {
    return "python";
  } else if (extension == ".cpp" || extension == ".c" || extension == ".h" ||
             extension == ".hpp") {
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
    return "cpp";
  } else {
    return "text";
  }
}
