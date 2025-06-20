#include <chrono>    // For current time
#include <ctime>     // For std::put_time, std::localtime
#include <iomanip>   // For std::put_time
#include <sstream>
#include <string>    // For std::string
#include <string>
#include <map>
#include <vector> // For parent hashes

class Commit {
public:
    std::string hash; // The hash of this commit object
    std::string message;
    std::string timestamp;
    std::string parentHash; // For simplicity, single parent for now. For merges, this could be a vector.
    std::map<std::string, std::string> fileBlobs; // Filename to blob hash mapping

    Commit(); // Default constructor
    Commit(const std::string& msg, const std::string& parent);

    std::string serialize() const; // Convert object to string for storage
    static Commit deserialize(const std::string& data); // Convert string back to object
    void computeAndSetHash(); // Computes hash based on serialized content
};

static std::string computeSimpleHash(const std::string& data) {
    unsigned long hash = 5381; // djb2 hash constant
    for (char c : data) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c); // hash * 33 + c
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}


Commit::Commit() : hash(""), message(""), timestamp(""), parentHash("") {}

Commit::Commit(const std::string& msg, const std::string& parent)
    : message(msg), parentHash(parent) {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    timestamp = ss.str();
}

std::string Commit::serialize() const {
    std::stringstream ss;
    ss << "message:" << message << "\n";
    ss << "timestamp:" << timestamp << "\n";
    ss << "parent:" << parentHash << "\n";
    ss << "files:";
    bool first = true;
    for (const auto& entry : fileBlobs) {
        if (!first) ss << ",";
        ss << entry.first << "=" << entry.second;
        first = false;
    }
    ss << "\n";
    return ss.str();
}

Commit Commit::deserialize(const std::string& data) {
    Commit c;
    std::stringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        if (key == "message") c.message = value;
        else if (key == "timestamp") c.timestamp = value;
        else if (key == "parent") c.parentHash = value;
        else if (key == "files") {
            std::stringstream filesSs(value);
            std::string fileEntry;
            while (std::getline(filesSs, fileEntry, ',')) {
                size_t eqPos = fileEntry.find('=');
                if (eqPos != std::string::npos) {
                    std::string filename = fileEntry.substr(0, eqPos);
                    std::string blobHash = fileEntry.substr(eqPos + 1);
                    c.fileBlobs[filename] = blobHash;
                }
            }
        }
    }
    return c;
}

void Commit::computeAndSetHash() {
    std::string contentToHash = "message:" + message + "\n" +
                                "timestamp:" + timestamp + "\n" +
                                "parent:" + parentHash + "\n" +
                                "files:";
    bool first = true;
    for (const auto& entry : fileBlobs) {
        if (!first) contentToHash += ",";
        contentToHash += entry.first + "=" + entry.second;
        first = false;
    }
    contentToHash += "\n";
    this->hash = computeSimpleHash(contentToHash); // Call the inlined function
}
