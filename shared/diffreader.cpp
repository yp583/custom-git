#include "diffreader.hpp"
#include <vector>

DiffReader::DiffReader(istream& in, bool verbose) 
    : in(in), 
      verbose(verbose), 
      diff_header_regex(regex("^diff --git a/(.*) b/(.*)")),
      in_file(false), 
      in_chunk(false), 
      curr_line_num(0)
{}
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
        this->curr_line_num = 0;

        this->in_file = true;
        this->in_chunk = false;
        this->files.push_back(current_file);

        if (this->verbose){
            cout << "LINE WAS NEW FILE: " << line << endl;
        }
        return;
    }

    // Check for chunk header (@@). This will skip some context for the diff (one line per chunk header). TODO: fix
    if (this->in_file && line.substr(0, 2) == "@@") {
        this->in_chunk = true;
        if (this->verbose){
            cout << "LINE WAS NEW CHUNK: " << line << endl;
        }
        return;
    }

    // Process diff lines if we're in a chunk
    if (this->in_file && this->in_chunk && !this->files.empty()) {
        DiffLine dline;
        dline.content = line.substr(1);
        dline.line_num = this->curr_line_num;


        if (this->verbose){
            cout << "LINE BEING ADDED: " << line << endl;
        }
        
        
        
        if (line[0] == '+') {
            dline.mode = INSERTION;
            // line_processed = true;
        } else if (line[0] == '-') {
            dline.mode = DELETION;
            // line_processed = true;
        } else if (line[0] == ' ') {
            // Context line (unchanged)
            dline.mode = EQ;
        }
        
        // if (line_processed) {
        // Add the line to the most recent file
        this->files.back().lines.push_back(dline);
        this->curr_line_num += 1;
    }
}

void DiffReader::ingestDiff() {
    string line;
    while (getline(this->in, line)) {
        this->ingestDiffLine(line);
    }
}

DiffReader::~DiffReader() {}

DiffChunk getDiffContent(DiffFile file, vector<DiffMode> types) {
    DiffChunk result;
    result.filepath = file.filepath;
    
    for (const DiffLine& line : file.lines) {
        // If types is empty, return all lines
        if (types.empty()) {
            result.lines.push_back(line);
        } else {
            // Check if line's mode is in the types vector
            for (const DiffMode& type : types) {
                if (line.mode == type) {
                    result.lines.push_back(line);
                    break;
                }
            }
        }
    }
    
    return result;
};

string combineContent(DiffChunk chunk) {
    string result = "";
    for (const DiffLine& line : chunk.lines) {
        result += line.content + "\n";
    }
    return result;
};