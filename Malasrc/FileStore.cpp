#include "FileStore.h"

#include <fstream>
#include <filesystem>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace risms {

Result<void> FileStore::ensureParentDirExists(const std::string& path) {
    fs::path p(path);
    fs::path parent = p.parent_path();
    if (parent.empty()) return Result<void>::Ok(); // path is relative, no dir component

    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec) {
        return Result<void>::Fail(ErrorCode::FileIOError,
                                   "Could not create directory " + parent.string() + ": " + ec.message());
    }
    return Result<void>::Ok();
}

std::string FileStore::makeTempPath(const std::string& path) {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;

    std::ostringstream oss;
    oss << path << ".tmp" << std::hex << dist(gen);
    return oss.str();
}

Result<std::vector<std::string>> FileStore::readLines(const std::string& path) {
    std::ifstream file(path);
    std::vector<std::string> lines;

    if (!file.is_open()) {
        // Not-yet-created file is a normal state, not an error.
        return Result<std::vector<std::string>>::Ok(lines);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) lines.push_back(line);
    }

    if (file.bad()) {
        return Result<std::vector<std::string>>::Fail(ErrorCode::FileIOError,
                                                        "Error while reading file: " + path);
    }

    return Result<std::vector<std::string>>::Ok(lines);
}

Result<void> FileStore::writeLinesAtomic(const std::string& path, const std::vector<std::string>& lines) {
    auto dirResult = ensureParentDirExists(path);
    if (dirResult.isError()) return dirResult;

    std::string tempPath = makeTempPath(path);

    {
        std::ofstream temp(tempPath, std::ios::trunc | std::ios::binary);
        if (!temp.is_open()) {
            return Result<void>::Fail(ErrorCode::FileIOError,
                                       "Could not open temp file for writing: " + tempPath);
        }
        for (const auto& line : lines) {
            temp << line << '\n';
        }
        temp.flush();
        if (!temp) {
            temp.close();
            std::error_code ec;
            fs::remove(tempPath, ec);
            return Result<void>::Fail(ErrorCode::FileIOError,
                                       "Error while writing temp file: " + tempPath);
        }
        // temp closes here (end of scope) before rename, required on some platforms.
    }

    std::error_code ec;
    fs::rename(tempPath, path, ec);
    if (ec) {
        // Clean up the orphaned temp file; the original file (if any) is untouched.
        std::error_code cleanupEc;
        fs::remove(tempPath, cleanupEc);
        return Result<void>::Fail(ErrorCode::FileIOError,
                                   "Could not finalize write to " + path + ": " + ec.message());
    }

    return Result<void>::Ok();
}

Result<void> FileStore::appendLine(const std::string& path, const std::string& line) {
    auto dirResult = ensureParentDirExists(path);
    if (dirResult.isError()) return dirResult;

    std::ofstream file(path, std::ios::app);
    if (!file.is_open()) {
        return Result<void>::Fail(ErrorCode::FileIOError,
                                   "Could not open file for appending: " + path);
    }
    file << line << '\n';
    if (!file) {
        return Result<void>::Fail(ErrorCode::FileIOError,
                                   "Error while appending to file: " + path);
    }
    return Result<void>::Ok();
}

} // namespace risms
