#include <iostream>
#include <string>
#include <array>
#include <memory>
#define LINE_MAX 128

using namespace std;

int main() {
    string command = "git diff --stat -U3 HEAD^^^ HEAD";
    FILE *fp;
    int status;
    char line[LINE_MAX];


    fp = popen("ls *", "r");
    if (fp == NULL) {
        cerr << "Error: Failed to execute command" << endl;
        return 1;
    }

    while (fgets(line, LINE_MAX, fp) != NULL){
        printf("%s", line);
    }
    
    status = pclose(fp);
    if (status == -1) {
        cerr << "Error: Failed to close pipe" << endl;
        return 1;
    } else {
        if (WIFEXITED(status)) {
            int exitStatus = WEXITSTATUS(status);
            if (exitStatus != 0) {
                cerr << "Command exited with status " << exitStatus << endl;
                return exitStatus;
            }
        } else if (WIFSIGNALED(status)) {
            cerr << "Command killed by signal " << WTERMSIG(status) << endl;
            return 1;
        }
    }
    // // Open a pipe to execute the command
    // unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    // if (!pipe) {
    //     cerr << "Error: Failed to execute command" << endl;
    //     return 1;
    // }
    
    // // Read the output
    // while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    //     result += buffer.data();
    // }
    
    // cout << result;
    return 0;
}