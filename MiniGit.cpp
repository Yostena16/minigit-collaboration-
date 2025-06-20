#include <string>
#include <map>
#include <vector>
#include <filesystem> // For direct filesystem operations
#include "Commit.cpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>     // For std::set in merge/LCA

namespace fs = std::filesystem; // Shorter alias for std::filesystem

// Define constants for repository structure
const std::string MINIGIT_DIR = ".minigit/";
const std::string OBJECTS_DIR = MINIGIT_DIR + "objects/";
const std::string REFS_DIR = MINIGIT_DIR + "refs/";
const std::string HEAD_FILE = REFS_DIR + "HEAD";
const std::string HEADS_DIR = REFS_DIR + "heads/";
const std::string INDEX_FILE = MINIGIT_DIR + "index"; // Staging area

class MiniGit {
private:
    // Inlined FileUtils methods
    bool createDirectory(const std::string& path);
    bool fileExists(const std::string& path);
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);
    bool removeFile(const std::string& path);

    // Helper methods for MiniGit logic
    std::map<std::string, std::string> readStagingArea();
    bool writeStagingArea(const std::map<std::string, std::string>& stagingArea);
    std::string getHeadCommitHash();
    bool updateHead(const std::string& commitHash);
    Commit readCommit(const std::string& commitHash);
    std::string getFileContentFromCommit(const Commit& commit, const std::string& filename);
    std::string findLCA(const std::string& commitHash1, const std::string& commitHash2);
    void writeBlob(const std::string& content, const std::string& blobHash);

public:

    bool initRepo(); // Corresponds to 'init'
    bool addFile(const std::string& filename); // Corresponds to 'add'
    bool makeCommit(const std::string& msg); // Corresponds to 'commit'
    void showLog(); // Corresponds to 'log'
    bool createBranch(const std::string& name); // Corresponds to 'branch'
    bool switchTo(const std::string& target); // Corresponds to 'checkout'
    bool mergeBranch(const std::string& name); // Corresponds to 'merge'
    void diffFiles(const std::string& f1, const std::string& f2); // Corresponds to 'diff'
};

bool MiniGit::createDirectory(const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        if (fs::create_directories(path, ec)) {
            return true;
        } else {
            std::cerr << "Error creating directory '" << path << "': " << ec.message() << std::endl;
            return false;
        }
    }
    if (fs::is_directory(path, ec)) {
        return true;
    }
    std::cerr << "Error: Path '" << path << "' exists but is not a directory." << std::endl;
    return false;
}

bool MiniGit::fileExists(const std::string& path) {
    return fs::exists(path);
}

std::string MiniGit::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    return content;
}

bool MiniGit::writeFile(const std::string& path, const std::string& content) {
    fs::path p = path;
    if (p.has_parent_path()) {
        if (!createDirectory(p.parent_path().string())) {
            std::cerr << "Error: Could not create parent directory for writing file: " << path << std::endl;
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << path << std::endl;
        return false;
    }
    file << content;
    file.close();
    return true;
}

bool MiniGit::removeFile(const std::string& path) {
    std::error_code ec;
    if (fs::is_regular_file(path, ec)) {
        fs::remove(path, ec);
        if (ec) {
            std::cerr << "Error removing file '" << path << "': " << ec.message() << std::endl;
            return false;
        }
        return true;
    }
    return true;
}

std::map<std::string, std::string> MiniGit::readStagingArea() {
    std::map<std::string, std::string> stagingArea;
    std::string content = readFile(INDEX_FILE);
    std::stringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            std::string filePath = line.substr(0, spacePos);
            std::string blobHash = line.substr(spacePos + 1);
            stagingArea[filePath] = blobHash;
        }
    }
    return stagingArea;
}

bool MiniGit::writeStagingArea(const std::map<std::string, std::string>& stagingArea) {
    std::stringstream ss;
    for (const auto& entry : stagingArea) {
        ss << entry.first << " " << entry.second << "\n";
    }
    return writeFile(INDEX_FILE, ss.str());
}

std::string MiniGit::getHeadCommitHash() {
    if (!fileExists(HEAD_FILE)) {
        return "";
    }
    std::string headContent = readFile(HEAD_FILE);
    if (headContent.empty()) return "";

    if (headContent.rfind("ref: ", 0) == 0) {
        std::string refPath = headContent.substr(5);
        if (!refPath.empty() && refPath.back() == '\n') {
            refPath.pop_back();
        }
        std::string branchRefFile = MINIGIT_DIR + refPath;
        std::string hash = readFile(branchRefFile);
        if (!hash.empty() && hash.back() == '\n') {
            hash.pop_back();
        }
        return hash;
    } else {
        if (!headContent.empty() && headContent.back() == '\n') {
            headContent.pop_back();
        }
        return headContent;
    }
}

bool MiniGit::updateHead(const std::string& commitHash) {
    std::string headContent = readFile(HEAD_FILE);
    if (headContent.rfind("ref: ", 0) == 0) {
        std::string refPath = headContent.substr(5);
        if (!refPath.empty() && refPath.back() == '\n') {
            refPath.pop_back();
        }
        std::string branchRefFile = MINIGIT_DIR + refPath;
        return writeFile(branchRefFile, commitHash + "\n");
    } else {
        return writeFile(HEAD_FILE, commitHash + "\n");
    }
}

Commit MiniGit::readCommit(const std::string& commitHash) {
    std::string commitPath = OBJECTS_DIR + commitHash;
    std::string commitData = readFile(commitPath);
    if (commitData.empty()) {
        return Commit();
    }
    return Commit::deserialize(commitData);
}

std::string MiniGit::getFileContentFromCommit(const Commit& commit, const std::string& filename) {
    auto it = commit.fileBlobs.find(filename);
    if (it != commit.fileBlobs.end()) {
        return readFile(OBJECTS_DIR + it->second);
    }
    return "";
}

std::string MiniGit::findLCA(const std::string& commitHash1, const std::string& commitHash2) {
    std::set<std::string> path1;
    std::string current = commitHash1;
    while (!current.empty()) {
        path1.insert(current);
        Commit c = readCommit(current);
        current = c.parentHash;
    }

    current = commitHash2;
    while (!current.empty()) {
        if (path1.count(current)) {
            return current;
        }
        Commit c = readCommit(current);
        current = c.parentHash;
    }
    return "";
}

void MiniGit::writeBlob(const std::string& content, const std::string& blobHash) {
    writeFile(OBJECTS_DIR + blobHash, content);
}

bool MiniGit::initRepo() {
    if (fileExists(MINIGIT_DIR)) {
        std::cout << "MiniGit repository already initialized in " << MINIGIT_DIR << std::endl;
        return true;
    }

    if (createDirectory(MINIGIT_DIR) &&
        createDirectory(OBJECTS_DIR) &&
        createDirectory(REFS_DIR) &&
        createDirectory(HEADS_DIR)) {

        if (writeFile(HEAD_FILE, "ref: refs/heads/master\n")) {
            if (writeFile(INDEX_FILE, "")) {
                if (writeFile(HEADS_DIR + "master", "\n")) {
                    std::cout << "Initialized empty MiniGit repository in " << MINIGIT_DIR << std::endl;
                    return true;
                }
            }
        }
    }
    std::cerr << "Failed to initialize MiniGit repository." << std::endl;
    return false;
}

bool MiniGit::addFile(const std::string& filename) {
    if (!fileExists(filename)) {
        std::cerr << "Error: File not found: " << filename << std::endl;
        return false;
    }
    if (!fileExists(MINIGIT_DIR)) {
        std::cerr << "Error: Not a MiniGit repository. Run 'minigit init' first." << std::endl;
        return false;
    }

    std::string fileContent = readFile(filename);
    std::string blobHash = computeSimpleHash(fileContent); // Call the inlined function

    writeBlob(fileContent, blobHash);

    std::map<std::string, std::string> stagingArea = readStagingArea();
    stagingArea[filename] = blobHash;
    if (!writeStagingArea(stagingArea)) {
        std::cerr << "Error: Could not update staging area for " << filename << std::endl;
        return false;
    }

    std::cout << "Added " << filename << " (blob: " << blobHash.substr(0, 7) << ")" << std::endl;
    return true;
}

bool MiniGit::makeCommit(const std::string& msg) {
    if (!fileExists(MINIGIT_DIR)) {
        std::cerr << "Error: Not a MiniGit repository. Run 'minigit init' first." << std::endl;
        return false;
    }

    std::map<std::string, std::string> stagingArea = readStagingArea();
    if (stagingArea.empty()) {
        std::cerr << "Nothing to commit, working tree clean." << std::endl;
        return false;
    }

    std::string parentHash = getHeadCommitHash();

    Commit newCommit(msg, parentHash);
    newCommit.fileBlobs = stagingArea;
    newCommit.computeAndSetHash();

    if (!writeFile(OBJECTS_DIR + newCommit.hash, newCommit.serialize())) {
        std::cerr << "Error: Could not write commit object." << std::endl;
        return false;
    }

    if (!updateHead(newCommit.hash)) {
        std::cerr << "Error: Could not update HEAD." << std::endl;
        return false;
    }

    if (!writeFile(INDEX_FILE, "")) {
        std::cerr << "Warning: Could not clear staging area after commit." << std::endl;
    }

    std::cout << "Committed: " << newCommit.hash.substr(0, 7) << " " << newCommit.message << std::endl;
    return true;
}

void MiniGit::showLog() {
    if (!fileExists(MINIGIT_DIR)) {
        std::cout << "No MiniGit repository found. Run 'minigit init' first." << std::endl;
        return;
    }

    std::string currentCommitHash = getHeadCommitHash();
    if (currentCommitHash.empty()) {
        std::cout << "No commits yet." << std::endl;
        return;
    }

    while (!currentCommitHash.empty()) {
        Commit commit = readCommit(currentCommitHash);
        std::cout << "commit " << commit.hash << std::endl;
        std::cout << "Date:   " << commit.timestamp << std::endl;
        std::cout << "    " << commit.message << std::endl;
        std::cout << std::endl;

        currentCommitHash = commit.parentHash;
    }
}

bool MiniGit::createBranch(const std::string& name) {
    if (!fileExists(MINIGIT_DIR)) {
        std::cerr << "Error: Not a MiniGit repository. Run 'minigit init' first." << std::endl;
        return false;
    }

    std::string currentCommitHash = getHeadCommitHash();
    if (currentCommitHash.empty()) {
        std::cerr << "Error: No commits to branch from. Create a commit first." << std::endl;
        return false;
    }

    std::string branchFilePath = HEADS_DIR + name;
    if (fileExists(branchFilePath)) {
        std::cerr << "Error: Branch '" << name << "' already exists." << std::endl;
        return false;
    }

    if (writeFile(branchFilePath, currentCommitHash + "\n")) {
        std::cout << "Created branch '" << name << "' pointing to " << currentCommitHash.substr(0, 7) << std::endl;
        return true;
    }
    std::cerr << "Error: Could not create branch file for '" << name << "'." << std::endl;
    return false;
}

bool MiniGit::switchTo(const std::string& target) {
    if (!fileExists(MINIGIT_DIR)) {
        std::cerr << "Error: Not a MiniGit repository. Run 'minigit init' first." << std::endl;
        return false;
    }

    std::string targetCommitHash;
    std::string branchPath = HEADS_DIR + target;

    if (fileExists(branchPath)) {
        targetCommitHash = readFile(branchPath);
        if (!targetCommitHash.empty() && targetCommitHash.back() == '\n') {
            targetCommitHash.pop_back();
        }
        if (targetCommitHash.empty()) {
             std::cerr << "Error: Branch '" << target << "' has no commits yet. Cannot switch to it." << std::endl;
             return false;
        }

        if (!writeFile(HEAD_FILE, "ref: refs/heads/" + target + "\n")) {
            std::cerr << "Error: Could not update HEAD to branch " << target << std::endl;
            return false;
        }
    } else {
        if (!fileExists(OBJECTS_DIR + target)) {
            std::cerr << "Error: Neither branch '" << target << "' nor commit '" << target << "' found." << std::endl;
            return false;
        }
        targetCommitHash = target;
        if (!writeFile(HEAD_FILE, targetCommitHash + "\n")) {
            std::cerr << "Error: Could not update HEAD to commit " << target << std::endl;
            return false;
        }
    }

    Commit targetCommit = readCommit(targetCommitHash);

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(".", ec)) {
        std::string currentPath = entry.path().string();
        if (currentPath == MINIGIT_DIR || fs::path(currentPath).filename() == "minigit" || fs::path(currentPath).filename() == "minigit.exe") continue;

        if (entry.is_directory(ec)) continue;

        std::string canonicalPath = fs::relative(entry.path(), fs::current_path()).string();

        if (fileExists(canonicalPath) &&
            targetCommit.fileBlobs.find(canonicalPath) == targetCommit.fileBlobs.end()) {
            removeFile(canonicalPath);
        }
    }

    for (const auto& entry : targetCommit.fileBlobs) {
        const std::string& filename = entry.first;
        const std::string& blobHash = entry.second;
        std::string blobContent = readFile(OBJECTS_DIR + blobHash);
        if (blobContent.empty() && !fileExists(OBJECTS_DIR + blobHash)) {
            std::cerr << "Warning: Blob " << blobHash << " for file " << filename << " not found. Skipping." << std::endl;
            continue;
        }

        if (!writeFile(filename, blobContent)) {
            std::cerr << "Error: Could not restore file " << filename << std::endl;
            return false;
        }
    }

    if (!writeFile(INDEX_FILE, "")) {
        std::cerr << "Warning: Could not clear staging area after checkout." << std::endl;
    }

    std::cout << "Switched to '" << target << "' (" << targetCommitHash.substr(0, 7) << ")" << std::endl;
    return true;
}

bool MiniGit::mergeBranch(const std::string& name) {
    if (!fileExists(MINIGIT_DIR)) {
        std::cerr << "Error: Not a MiniGit repository. Run 'minigit init' first." << std::endl;
        return false;
    }

    std::string currentBranchCommitHash = getHeadCommitHash();
    std::string targetBranchPath = HEADS_DIR + name;

    if (!fileExists(targetBranchPath)) {
        std::cerr << "Error: Branch '" << name << "' does not exist." << std::endl;
        return false;
    }

    std::string targetBranchCommitHash = readFile(targetBranchPath);
    if (!targetBranchCommitHash.empty() && targetBranchCommitHash.back() == '\n') {
        targetBranchCommitHash.pop_back();
    }

    if (currentBranchCommitHash.empty() || targetBranchCommitHash.empty()) {
        std::cerr << "Error: One of the branches has no commits to merge." << std::endl;
        return false;
    }

    if (currentBranchCommitHash == targetBranchCommitHash) {
        std::cout << "Already up to date." << std::endl;
        return true;
    }

    std::string lcaHash = findLCA(currentBranchCommitHash, targetBranchCommitHash);
    if (lcaHash.empty()) {
        std::cerr << "Error: Could not find a common ancestor for merge." << std::endl;
        return false;
    }

    Commit lcaCommit = readCommit(lcaHash);
    Commit currentCommit = readCommit(currentBranchCommitHash);
    Commit targetCommit = readCommit(targetBranchCommitHash);

    std::map<std::string, std::string> mergedFileBlobs = currentCommit.fileBlobs;
    bool conflictDetected = false;
    std::error_code ec;

    std::set<std::string> allFiles;
    for (const auto& entry : lcaCommit.fileBlobs) allFiles.insert(entry.first);
    for (const auto& entry : currentCommit.fileBlobs) allFiles.insert(entry.first);
    for (const auto& entry : targetCommit.fileBlobs) allFiles.insert(entry.first);

    for (const std::string& filename : allFiles) {
        std::string lcaContent = getFileContentFromCommit(lcaCommit, filename);
        std::string currentContent = getFileContentFromCommit(currentCommit, filename);
        std::string targetContent = getFileContentFromCommit(targetCommit, filename);

        bool inLCA = lcaCommit.fileBlobs.count(filename);
        bool inCurrent = currentCommit.fileBlobs.count(filename);
        bool inTarget = targetCommit.fileBlobs.count(filename);

        if (inCurrent && inTarget) {
            if (currentContent == targetContent) {
                std::string newBlobHash = computeSimpleHash(currentContent);
                writeBlob(currentContent, newBlobHash);
                mergedFileBlobs[filename] = newBlobHash;
                writeFile(filename, currentContent);
            } else if (currentContent == lcaContent) {
                std::string newBlobHash = computeSimpleHash(targetContent);
                writeBlob(targetContent, newBlobHash);
                mergedFileBlobs[filename] = newBlobHash;
                writeFile(filename, targetContent);
            } else if (targetContent == lcaContent) {
                std::string newBlobHash = computeSimpleHash(currentContent);
                writeBlob(currentContent, newBlobHash);
                mergedFileBlobs[filename] = newBlobHash;
                writeFile(filename, currentContent);
            } else {
                conflictDetected = true;
                std::cerr << "CONFLICT: both modified " << filename << std::endl;
                std::string conflictContent = "<<<<<<< HEAD\n" + currentContent +
                                              "=======\n" + targetContent +
                                              ">>>>>>> " + name + "\n";
                std::string conflictBlobHash = computeSimpleHash(conflictContent);
                writeBlob(conflictContent, conflictBlobHash);
                writeFile(filename, conflictContent);
                mergedFileBlobs[filename] = conflictBlobHash;
            }
        } else if (inCurrent && !inTarget) {
            if (inLCA && lcaContent == currentContent) {
                mergedFileBlobs.erase(filename);
                removeFile(filename);
            } else {
                std::string newBlobHash = computeSimpleHash(currentContent);
                writeBlob(currentContent, newBlobHash);
                mergedFileBlobs[filename] = newBlobHash;
                writeFile(filename, currentContent);
            }
        } else if (!inCurrent && inTarget) {
            if (inLCA && lcaContent == targetContent) {
                mergedFileBlobs.erase(filename);
                removeFile(filename);
            } else {
                std::string newBlobHash = computeSimpleHash(targetContent);
                writeBlob(targetContent, newBlobHash);
                mergedFileBlobs[filename] = newBlobHash;
                writeFile(filename, targetContent);
            }
        } else if (!inLCA && inCurrent && !inTarget) {
            std::string newBlobHash = computeSimpleHash(currentContent);
            writeBlob(currentContent, newBlobHash);
            mergedFileBlobs[filename] = newBlobHash;
            writeFile(filename, currentContent);
        } else if (!inLCA && !inCurrent && inTarget) {
            std::string newBlobHash = computeSimpleHash(targetContent);
            writeBlob(targetContent, newBlobHash);
            mergedFileBlobs[filename] = newBlobHash;
            writeFile(filename, targetContent);
        }
    }

    if (conflictDetected) {
        std::cout << "Automatic merge failed; fix conflicts in working directory, then 'minigit add .' and 'minigit commit -m \"Merge...\"'." << std::endl;
    } else {
        std::cout << "Merge successful." << std::endl;

        std::map<std::string, std::string> newStagingArea;
        for (const auto& entry : mergedFileBlobs) {
            std::string content = readFile(entry.first);
            std::string newBlobHash = computeSimpleHash(content);
            writeBlob(content, newBlobHash);
            newStagingArea[entry.first] = newBlobHash;
        }
        writeStagingArea(newStagingArea);

        if (!conflictDetected) {
            std::string msg = "Merge branch '" + name + "' into " + getHeadCommitHash();
            makeCommit(msg);
        }
    }
    return true;
}


void MiniGit::diffFiles(const std::string& f1, const std::string& f2) {
    std::ifstream a(f1), b(f2);
    if (!a.is_open() || !b.is_open()) {
        std::cerr << "Error: Could not open one or both files for diff: " << f1 << ", " << f2 << std::endl;
        return;
    }

    std::string la, lb;
    int line = 1;
    bool hasDiff = false;
    while (true) {
        bool readA = static_cast<bool>(getline(a, la));
        bool readB = static_cast<bool>(getline(b, lb));

        if (!readA && !readB) break;

        if (readA && readB) {
            if (la != lb) {
                std::cout << "Line " << line << ":\n";
                std::cout << "< " << la << "\n";
                std::cout << "> " << lb << "\n";
                hasDiff = true;
            }
        } else if (readA) {
            std::cout << "Line " << line << ":\n";
            std::cout << "< " << la << "\n";
            hasDiff = true;
        } else if (readB) {
            std::cout << "Line " << line << ":\n";
            std::cout << "> " << lb << "\n";
            hasDiff = true;
        }
        line++;
    }
    if (!hasDiff) {
        std::cout << "Files are identical.\n";
    }
}
