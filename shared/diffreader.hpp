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
    DELETION = 2
};
struct DiffLine {
    DiffMode mode;
    string content;
    int line_num;
};
struct DiffFile {
    string filepath;
    vector<DiffLine> lines;
    int old_start = 1;
    int new_start = 1;
};
//same data different purpose
struct DiffChunk {
    string filepath;
    vector<DiffLine> lines;
    int old_start = 1;
    int new_start = 1;
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

    vector<DiffFile> files;

    void ingestDiffLine(string line);

public:
    DiffReader(istream& in, bool verbose = false);
    vector<DiffFile> getFiles() const;
    void ingestDiff();
    ~DiffReader();
};

DiffChunk getDiffContent(DiffFile file, vector<DiffMode> types);
string combineContent(DiffChunk chunk);
string createPatch(DiffChunk chunk, int old_offset = 0, int new_offset = 0, bool include_file_header = true);
vector<string> createPatches(vector<DiffChunk> chunks);

#endif // DIFFREADER_HPP
