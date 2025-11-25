#include "diffreader.hpp"
#include <vector>
#include <fstream>

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
        // Store filepath, wait for @@ to create DiffFile
        this->current_filepath = match[2].str();
        this->curr_line_num = 0;

        this->in_file = true;
        this->in_chunk = false;

        if (this->verbose){
            cout << "LINE WAS NEW FILE: " << line << endl;
        }
        return;
    }

    // Check for chunk header (@@) - create new DiffFile for each hunk
    if (this->in_file && line.substr(0, 2) == "@@") {
        this->in_chunk = true;

        DiffFile current_file = DiffFile{};
        current_file.filepath = this->current_filepath;

        // Parse: @@ -old_start,old_count +new_start,new_count @@
        regex hunk_regex("^@@ -(\\d+),?(\\d*) \\+(\\d+),?(\\d*) @@");
        smatch m;
        if (regex_search(line, m, hunk_regex)) {
            current_file.old_start = stoi(m[1].str());
            current_file.new_start = stoi(m[3].str());
        }

        this->files.push_back(current_file);

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
        } else if (line[0] == '-') {
            dline.mode = DELETION;
        } else if (line[0] == ' ') {
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
    result.old_start = file.old_start;
    result.new_start = file.new_start;

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

int getNumLines(string filepath) {
    ifstream rFile(filepath);

    if (!rFile.is_open()) {
        cerr << "Error opening file!" << endl;
        return -1;  // Use -1 for error (0 could be valid)
    }

    int count = 0;
    string line;
    while (getline(rFile, line)) {
        count++;
    }

    return count;
}

string createPatch(DiffChunk chunk) {
    string patch;

    patch += "--- a/" + chunk.filepath + "\n";
    patch += "+++ b/" + chunk.filepath + "\n";

    // Calculate counts
    int old_count = 0, new_count = 0;
    for (const DiffLine& line : chunk.lines) {
        if (line.mode == EQ)        { old_count++; new_count++; }
        else if (line.mode == DELETION)  { old_count++; }
        else if (line.mode == INSERTION) { new_count++; }
    }

    // Hunk header
    patch += "@@ -" + to_string(chunk.old_start) + "," + to_string(old_count) +
             " +" + to_string(chunk.new_start) + "," + to_string(new_count) + " @@\n";

    // Lines with prefixes
    for (const DiffLine& line : chunk.lines) {
        switch (line.mode) {
            case EQ:        patch += " " + line.content + "\n"; break;
            case INSERTION: patch += "+" + line.content + "\n"; break;
            case DELETION:  patch += "-" + line.content + "\n"; break;
        }
    }

    return patch;
}
