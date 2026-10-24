#ifndef RISMS_FILE_STORE_H
#define RISMS_FILE_STORE_H

#include <string>
#include <vector>
#include "Result.h"

namespace risms {

// Low-level local-file persistence helpers shared by every repository
// (UserRepository today, ProductRepository later). Two access patterns:
//
//  - readLines/writeLinesAtomic: for files that are rewritten as a whole
//    on every save (e.g. users.txt, products.txt). The write goes to a
//    temp file in the same directory and is then renamed over the target,
//    so a crash or power loss mid-write leaves the original file intact
//    instead of a half-written/corrupt one.
//
//  - appendLine: for files that only ever grow (e.g. a stock movement
//    history log), where rewriting the whole file on every entry would be
//    wasteful.
class FileStore {
public:
    // Reads path as a list of non-empty lines (blank lines are skipped,
    // trailing newline is not included). A missing file is NOT an error -
    // it returns Ok({}) so callers can treat "file not created yet" the
    // same as "file exists but empty".
    static Result<std::vector<std::string>> readLines(const std::string& path);

    // Atomically replaces the contents of path with lines (one per line,
    // newline-terminated). Creates parent directories if needed. On
    // success, readers will only ever see either the old content or the
    // fully-written new content, never a partial write.
    static Result<void> writeLinesAtomic(const std::string& path, const std::vector<std::string>& lines);

    // Appends a single line (plus trailing newline) to path, creating the
    // file (and parent directories) if it doesn't exist yet.
    static Result<void> appendLine(const std::string& path, const std::string& line);

private:
    static Result<void> ensureParentDirExists(const std::string& path);
    static std::string makeTempPath(const std::string& path);
};

} // namespace risms

#endif // RISMS_FILE_STORE_H
