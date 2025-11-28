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

    if (regex_match(line, match, this->diff_header_regex)) {
        this->current_old_filepath = match[1].str();
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

    regex deleted_regex("^deleted file mode");
    if (this->in_file && regex_search(line, deleted_regex)) {
        this->current_is_deleted = true;
        if (this->verbose){
            cout << "FILE MARKED AS DELETED: " << line << endl;
        }
        return;
    }

    regex new_file_regex("^new file mode");
    if (this->in_file && regex_search(line, new_file_regex)) {
        this->current_is_new = true;
        if (this->verbose){
            cout << "FILE MARKED AS NEW: " << line << endl;
        }
        return;
    }

    if (this->in_file && line.substr(0, 2) == "@@") {
        this->in_chunk = true;

        DiffChunk current_chunk = DiffChunk{};
        current_chunk.filepath = this->current_filepath;
        current_chunk.old_filepath = this->current_old_filepath;
        current_chunk.is_deleted = this->current_is_deleted;
        current_chunk.is_new = this->current_is_new;

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
            dline.content = line;
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
        return -1;
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
        bool is_rename = (chunk.old_filepath != chunk.filepath) && !chunk.is_new && !chunk.is_deleted;

        if (is_rename) {
            patch += "diff --git a/" + chunk.old_filepath + " b/" + chunk.filepath + "\n";
            patch += "rename from " + chunk.old_filepath + "\n";
            patch += "rename to " + chunk.filepath + "\n";
        }

        if (chunk.is_new) {
            patch += "--- /dev/null\n";
        } else {
            patch += "--- a/" + chunk.old_filepath + "\n";
        }
        if (chunk.is_deleted) {
            patch += "+++ /dev/null\n";
        } else {
            patch += "+++ b/" + chunk.filepath + "\n";
        }
    }

    int old_count = 0, new_count = 0;
    for (const DiffLine& line : chunk.lines) {
        if (line.mode == EQ)             { old_count++; new_count++; }
        else if (line.mode == DELETION)  { old_count++; }
        else if (line.mode == INSERTION) { new_count++; }
    }

    if (old_count == 0 && new_count == 0) {
        return "";
    }

    patch += "@@ -" + to_string(chunk.start) + "," + to_string(old_count) +
             " +" + to_string(chunk.start) + "," + to_string(new_count) + " @@\n";

    for (const DiffLine& line : chunk.lines) {
        switch (line.mode) {
            case EQ:        patch += " " + line.content + "\n"; break;
            case INSERTION: patch += "+" + line.content + "\n"; break;
            case DELETION:  patch += "-" + line.content + "\n"; break;
            case NO_NEWLINE: patch += line.content + "\n"; break;
        }
    }

    return patch;
}

vector<string> createPatches(vector<DiffChunk> chunks) {
    vector<string> patches;
    unordered_map<string, string> renamed_files;

    for (DiffChunk chunk : chunks) {
        auto it = renamed_files.find(chunk.old_filepath);
        if (it != renamed_files.end()) {
            chunk.old_filepath = it->second;
            chunk.filepath = it->second;
        }

        if (chunk.old_filepath != chunk.filepath && !chunk.is_new && !chunk.is_deleted) {
            renamed_files[chunk.old_filepath] = chunk.filepath;
        }

        patches.push_back(createPatch(chunk, true));
    }

    return patches;
}
