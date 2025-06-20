#include "MiniGit.cpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace fs = std::filesystem;

// Colors for console output
#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define YEL  "\x1B[33m"
#define BLU  "\x1B[34m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define WHT  "\x1B[37m"
#define END  "\033[0m"

using namespace std;

void printUsage(){
    cout << BLU "Usage: " << endl;
    cout << "./minigit init                               ->   initialize an empty git repository in the current dir" << endl;
    cout << "./minigit add <'.'|'file_name(s)'>           ->   add the file(s) to staging area ('.' for all files)" << endl;
    cout << "./minigit commit -m <'commit message'>       ->   commit your staging files" << endl;
    cout << "./minigit log                                ->   show commit log" << endl;
    cout << "./minigit branch <branch_name>               ->   create a new branch" << endl;
    cout << "./minigit checkout <branch_name_or_commit_hash> ->   checkout to a branch or checkout a commit" << endl;
    cout << "./minigit merge <branch_name>                ->   merge changes from another branch" << endl;
    cout << "./minigit diff <file1> <file2>               ->   show differences between two files" << END << endl;
}
int main(int argc, char *argv[]) {
    MiniGit mgit;

    if (argc >= 2) {
        string command = string(argv[1]);

        if (command == "init") {
            mgit.initRepo();
        } else if (command == "add") {
            if (argc < 3) {
                cout << RED "missing arguments!" << endl;
                cout << "Provide a file or '.' to add all files in current directory e.g." << endl;
                cout << "./minigit add <file_name> or ./minigit add ." END << endl;
            } else {
                string target = string(argv[2]);
                if (target == ".") {
                    error_code ec;
                    for (const auto& entry : fs::directory_iterator(".", ec)) {
                        if (entry.is_regular_file(ec)) {
                            string filePath = entry.path().string();
                            if (fs::path(filePath).filename() != "minigit" && fs::path(filePath).filename() != "./minigit" && filePath.find(MINIGIT_DIR) != 0) {
                                mgit.addFile(filePath);
                            }
                        }
                    }
                    if (ec) {
                        cout << RED "Error listing files in current directory: " << ec.message() <<END << endl;
                    }
                } else {
                    for (int i = 2; i < argc; ++i) {
                        mgit.addFile(string(argv[i]));
                    }
                }
            }
        } else if (command == "commit") {
            if (argc == 4 && string(argv[2]) == "-m") {
                string message = string(argv[3]);
                mgit.makeCommit(message);
            } else {
                cout << RED "missing arguments!" << endl;
                cout << "Provide with a message field e.g." << endl;
                cout << "./minigit commit -m 'my commit message'" END << endl;
            }
        } else if (command == "log") {
            mgit.showLog();
        } else if (command == "branch") {
            if (argc < 3) {
                cout << RED "missing arguments!" << endl;
                cout << "Provide a branch name e.g." << endl;
                cout << "./minigit branch <branch_name>" END << endl;
            } else {
                string name = string(argv[2]);
                mgit.createBranch(name);
            }
        } else if (command == "checkout") {
            if (argc < 3) {
                cout << RED "missing arguments!" << endl;
                cout << "Provide a branch name or commit hash e.g." << endl;
                cout << "./minigit checkout <branch_name_or_commit_hash>" END << endl;
            } else {
                string target = string(argv[2]);
                mgit.switchTo(target);
            }
        } else if (command == "merge") {
            if (argc < 3) {
                cout << RED "missing arguments!" << endl;
                cout << "Provide a branch name to merge from e.g." << endl;
                cout << "./minigit merge <branch_name>" END << endl;
            } else {
                string branchToMerge = string(argv[2]);
                mgit.mergeBranch(branchToMerge);
            }
        } else if (command == "diff") {
            if (argc < 4) {
                cout << RED "missing arguments!" << endl;
                cout << "Provide two file paths e.g." << endl;
                cout << "./minigit diff <file1> <file2>" END << endl;
            } else {
                string file1 = string(argv[2]);
                string file2 = string(argv[3]);
                mgit.diffFiles(file1, file2);
            }
        } else {
            cout << RED "Invalid command: " << command << END << endl;
            printUsage();
        }
    } else {
        cout << "minigit is a version control system. This project is a clone of git with minimal features.\n\n";
        printUsage();
    }

    return 0;
}
