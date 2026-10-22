#ifndef RISMS_CONFIG_STORE_H
#define RISMS_CONFIG_STORE_H

#include <string>
#include <unordered_map>
#include "Result.h"

namespace risms {

// Loads simple key=value configuration from a text file (blank lines and
// lines starting with '#' are ignored). Singleton so every component reads
// the same loaded config without passing it around everywhere.
class ConfigStore {
public:
    static ConfigStore& instance();

    // Reads and parses the file at path, replacing any previously loaded
    // values. Returns FileIOError if the file can't be opened.
    Result<void> load(const std::string& path);

    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue) const;

    ConfigStore(const ConfigStore&) = delete;
    ConfigStore& operator=(const ConfigStore&) = delete;

private:
    ConfigStore() = default;
    std::unordered_map<std::string, std::string> values_;
};

} // namespace risms

#endif // RISMS_CONFIG_STORE_H
