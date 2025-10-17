#include "diffreader.hpp"
#include <vector>

DiffReader::DiffReader(istream& in) : in(in), in_file(false), in_chunk(false) {
    this->diff_header_regex = regex("^diff --git a/(.*) b/(.*)");
    this->chunk_header_regex = regex("^@@.*@@");
    this->ins_regex = regex("^\\+(?!\\+)(.*)");
    this->del_regex = regex("^\\-(?!\\-)(.*)");
}

vector<DiffFile> DiffReader::getFiles() const {
    return this->files;
}

void DiffReader::ingestDiffLine(string line) {    
    smatch match;
    
    // Check for new file header
    if (regex_match(line, match, this->diff_header_regex)) {

        // Start new file
        DiffFile current_file = DiffFile{};
        current_file.filepath = match[2].str(); // Use 'b/' path (after changes)

        this->in_file = true;
        this->in_chunk = false;
        this->files.push_back(current_file);
        return;
    }

    // Check for chunk header (@@)
    if (this->in_file && regex_match(line, this->chunk_header_regex)) {
        this->in_chunk = true;
        return;
    }

    // Process diff lines if we're in a chunk
    if (this->in_file && this->in_chunk && !this->files.empty()) {
        DiffLine dline;
        bool line_processed = false;
        
        if (regex_match(line, match, this->ins_regex)) {
            dline.mode = INSERTION;
            dline.content = match[1].str();
            line_processed = true;
        } else if (regex_match(line, match, this->del_regex)) {
            dline.mode = DELETION;
            dline.content = match[1].str();
            line_processed = true;
        } else if (line[0] == ' ') {
            // Context line (unchanged)
            dline.mode = EQ;
            dline.content = line.substr(1); // Remove the leading space
            line_processed = true;
        }
        
        if (line_processed) {
            // Add the line to the most recent file
            this->files.back().lines.push_back(dline);
        }
    }
}

void DiffReader::ingestDiff() {
    string line;
    while (getline(this->in, line)) {
        this->ingestDiffLine(line);
    }
}

DiffReader::~DiffReader() {}