#include "diffreader.hpp"
#include <vector>
#include <fstream>

DiffReader::DiffReader(istream& in, bool verbose)
    : in(in),
      verbose(verbose),
      diff_header_regex(regex("^diff --git a/(.*) b/(.*)")),
      in_file(false),
      in_chunk(false),
      curr_line_num(0),
      current_is_deleted(false),
      current_is_new(false)
{}
vector<DiffChunk> DiffReader::getChunks() const {
    return this->chunks;
}

void DiffReader::ingestDiffLine(string line) {
    smatch match;

    // Check for new file header
    if (regex_match(line, match, this->diff_header_regex)) {
        // Store filepath, wait for @@ to create DiffFile
        this->current_filepath = match[2].str();
        this->curr_line_num = 0;
        this->current_is_deleted = false;
        this->current_is_new = false;

        this->in_file = true;
        this->in_chunk = false;

        if (this->verbose){
            cout << "LINE WAS NEW FILE: " << line << endl;
        }
        return;
    }

    // Check for file deletion marker
    regex deleted_regex("^deleted file mode");
    if (this->in_file && regex_search(line, deleted_regex)) {
        this->current_is_deleted = true;
        if (this->verbose){
            cout << "FILE MARKED AS DELETED: " << line << endl;
        }
        return;
    }

    // Check for new file marker
    regex new_file_regex("^new file mode");
    if (this->in_file && regex_search(line, new_file_regex)) {
        this->current_is_new = true;
        if (this->verbose){
            cout << "FILE MARKED AS NEW: " << line << endl;
        }
        return;
    }

    // Check for chunk header (@@) - create new DiffChunk for each hunk
    if (this->in_file && line.substr(0, 2) == "@@") {
        this->in_chunk = true;

        DiffChunk current_chunk = DiffChunk{};
        current_chunk.filepath = this->current_filepath;
        current_chunk.is_deleted = this->current_is_deleted;
        current_chunk.is_new = this->current_is_new;

        // Parse: @@ -start,old_count +new_start,new_count @@
        regex hunk_regex("^@@ -(\\d+),?(\\d*) \\+(\\d+),?(\\d*) @@");
        smatch m;
        if (regex_search(line, m, hunk_regex)) {
            current_chunk.start = stoi(m[1].str());
        }

        this->chunks.push_back(current_chunk);

        if (this->verbose){
            cout << "LINE WAS NEW CHUNK: " << line << endl;
        }
        return;
    }

    // Process diff lines if we're in a chunk
    if (this->in_file && this->in_chunk && !this->chunks.empty()) {
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
        } else if (line[0] == '\\') {
            dline.mode = NO_NEWLINE;
            dline.content = line;  // Keep full line for "\ No newline at end of file"
        }

        this->chunks.back().lines.push_back(dline);
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

string createPatch(DiffChunk chunk, bool include_file_header) {
    string patch;

    if (include_file_header) {
        // New file: first chunk uses /dev/null as source
        if (chunk.is_new) {
            patch += "--- /dev/null\n";
        } else {
            patch += "--- a/" + chunk.filepath + "\n";
        }
        // Deleted file: last chunk uses /dev/null as destination
        if (chunk.is_deleted) {
            patch += "+++ /dev/null\n";
        } else {
            patch += "+++ b/" + chunk.filepath + "\n";
        }
    }

    // Calculate counts (NO_NEWLINE marker doesn't count as a line)
    int old_count = 0, new_count = 0;
    for (const DiffLine& line : chunk.lines) {
        if (line.mode == EQ)             { old_count++; new_count++; }
        else if (line.mode == DELETION)  { old_count++; }
        else if (line.mode == INSERTION) { new_count++; }
        // NO_NEWLINE doesn't count
    }

    // Hunk header
    patch += "@@ -" + to_string(chunk.start) + "," + to_string(old_count) +
             " +" + to_string(chunk.start) + "," + to_string(new_count) + " @@\n";

    // Lines with prefixes
    for (const DiffLine& line : chunk.lines) {
        switch (line.mode) {
            case EQ:        patch += " " + line.content + "\n"; break;
            case INSERTION: patch += "+" + line.content + "\n"; break;
            case DELETION:  patch += "-" + line.content + "\n"; break;
            case NO_NEWLINE: patch += line.content + "\n"; break;  // Output as-is (already has \)
        }
    }

    return patch;
}

vector<string> createPatches(vector<DiffChunk> chunks) {
    vector<string> patches;

    for (const DiffChunk& chunk : chunks) {
        patches.push_back(createPatch(chunk, true));
    }

    return patches;
}
