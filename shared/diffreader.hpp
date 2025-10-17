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
};
struct DiffFile {
    string filepath;
    vector<DiffLine> lines;
};
class DiffReader {
private:
    istream& in;
    bool verbose;

    regex diff_header_regex;
    regex chunk_header_regex;
    regex ins_regex;
    regex del_regex;

    bool in_file;
    bool in_chunk;

    vector<DiffFile> files;

    void ingestDiffLine(string line);

public:
    DiffReader(istream& in, bool verbose = false);
    vector<DiffFile> getFiles() const;
    void ingestDiff();
    ~DiffReader();
};