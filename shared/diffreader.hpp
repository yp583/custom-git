#ifndef DIFFREADER_HPP
#define DIFFREADER_HPP

#include <iostream>
#include <string>
#include <regex>
#include <vector>
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
};
//same data different purpose
struct DiffChunk {
    string filepath;
    vector<DiffLine> lines;
};
class DiffReader {
private:
    istream& in;
    bool verbose;

    regex diff_header_regex;

    bool in_file;
    bool in_chunk;
    int curr_line_num;

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

#endif // DIFFREADER_HPP
