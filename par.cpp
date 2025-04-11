// iso_tp_multithread_ordered.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <map>
#include <algorithm>
// ---------------------------------------------------------------------
struct IsoTpMessage {
    bool assembling = false;
    int expectedLen = 0;
    int receivedLen = 0;
    int lastSN = -1;
    std::vector<uint8_t> data;
};
std::mutex outputMutex;
std::vector<std::tuple<int, unsigned int, std::string>> outputBuffer;
// ---------------------------------------------------------------------
bool isHexString(const std::string &s) {
    for (char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}
// ---------------------------------------------------------------------
std::vector<uint8_t> hexStringToBytes(const std::string &hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
    }
    return bytes;
}
// ---------------------------------------------------------------------
std::string bytesToHexString(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : data) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}
// ---------------------------------------------------------------------
void processCanFrames(unsigned int canId, const std::vector<std::pair<int, std::string>>& entries) {
    IsoTpMessage msg;
    for (const auto& [index, line] : entries) {
        if (line.size() != 19) continue;
        std::string payloadHex = line.substr(3);
        if (!isHexString(payloadHex)) continue;
        auto payload = hexStringToBytes(payloadHex);
        if (payload.empty()) continue;

        uint8_t pci = payload[0];
        uint8_t frameType = (pci >> 4) & 0x0F;
        uint8_t lowNibble = pci & 0x0F;

        switch (frameType) {
            case 0: { // SF
                int length = lowNibble;
                if (length <= 0 || length > 7 || payload.size() < size_t(length + 1)) break;
                std::vector<uint8_t> data(payload.begin() + 1, payload.begin() + 1 + length);
                std::lock_guard<std::mutex> lock(outputMutex);
                outputBuffer.emplace_back(index, canId, bytesToHexString(data));
                break;
            }
            case 1: { // FF
                if (payload.size() < 2) break;
                int totalLen = ((lowNibble & 0x0F) << 8) | payload[1];
                msg = IsoTpMessage();
                msg.assembling = true;
                msg.expectedLen = totalLen;
                msg.receivedLen = int(payload.size() - 2);
                msg.lastSN = 0;
                msg.data.insert(msg.data.end(), payload.begin() + 2, payload.end());
                break;
            }
            case 2: { // CF
                if (!msg.assembling) break;
                uint8_t sn = lowNibble;
                if (sn != ((msg.lastSN + 1) & 0x0F)) break; 
                msg.lastSN = sn;
                msg.data.insert(msg.data.end(), payload.begin() + 1, payload.end());
                msg.receivedLen += int(payload.size() - 1);
                if (msg.receivedLen >= msg.expectedLen) {
                    msg.data.resize(msg.expectedLen);
                    std::lock_guard<std::mutex> lock(outputMutex);
                    outputBuffer.emplace_back(index, canId, bytesToHexString(msg.data));
                    msg = IsoTpMessage();
                }
                break;
            }
            case 3: { // FC
                std::string fs = (lowNibble == 0) ? "CTS" : (lowNibble == 1) ? "WT" : (lowNibble == 2) ? "OVFLW" : "RES";
                std::ostringstream oss;
                oss << std::hex << canId << ": FC [" << fs << "], BlockSize=" << (int)payload[1] << ", STmin=" << (int)payload[2];
                std::lock_guard<std::mutex> lock(outputMutex);
                outputBuffer.emplace_back(index, canId, oss.str());
                break;
            }
            default:
                break;
        }
    }
}
// ---------------------------------------------------------------------
int main() {
    std::ifstream inFile("transcript.txt");
    if (!inFile.is_open()) {
        std::cerr << "Can't open transcript.txt" << std::endl;
        return 1;
    }

    std::unordered_map<unsigned int, std::vector<std::pair<int, std::string>>> canMap;
    std::string line;
    int lineIndex = 0;
    while (std::getline(inFile, line)) {
        if (line.size() != 19) continue;
        std::string idStr = line.substr(0, 3);
        if (!isHexString(idStr)) continue;
        unsigned int canId = std::stoul(idStr, nullptr, 16);
        canMap[canId].emplace_back(lineIndex++, line);
    }

    std::vector<std::thread> threads;
    for (auto& kv : canMap) {
        threads.emplace_back(processCanFrames, kv.first, kv.second);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::sort(outputBuffer.begin(), outputBuffer.end());
    for (const auto& [idx, canId, msg] : outputBuffer) {
        std::cout << std::hex << canId << ": " << msg << std::endl;
    }

    return 0;
}
// ---------------------------------------------------------------------