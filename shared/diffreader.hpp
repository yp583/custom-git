#ifndef DIFFREADER_HPP
#define DIFFREADER_HPP

#include <iostream>
#include <string>
#include <regex>
#include <vector>
#include <unordered_map>
#include <algorithm>
using namespace std;
enum DiffMode {
    EQ = 0,
    INSERTION = 1,
    DELETION = 2,
    NO_NEWLINE = 3
};
struct DiffLine {
    DiffMode mode;
    string content;
    int line_num;
};
struct DiffChunk {
    string filepath;
    vector<DiffLine> lines;
    int start = 1;
};


class DiffReader {
private:
    istream& in;
    bool verbose;

    regex diff_header_regex;

    bool in_file;
    bool in_chunk;
    int curr_line_num;
    string current_filepath;

    vector<DiffChunk> chunks;

    void ingestDiffLine(string line);

public:
    DiffReader(istream& in, bool verbose = false);
    vector<DiffChunk> getChunks() const;
    void ingestDiff();
    ~DiffReader();
};

string combineContent(DiffChunk chunk);
string createPatch(DiffChunk chunk, bool include_file_header = true);
vector<string> createPatches(vector<DiffChunk> chunks);

#endif // DIFFREADER_HPP
