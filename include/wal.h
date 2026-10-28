#pragma once
#include "common.h"

#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace minidb {

struct LogRecord {
    enum class Type { Create, Begin, Change, Commit, Abort };
    Type type = Type::Begin;
    uint64_t lsn = 0;
    uint64_t txn = 0;
    std::string table;
    int64_t key = 0;
    std::optional<Row> before;
    std::optional<Row> after;
};

class WAL {
public:
    WAL(const std::string& path, bool truncate);
    ~WAL();
    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;

    uint64_t append(LogRecord rec, bool force);
    void reset();
    static std::vector<LogRecord> readAll(const std::string& path);

private:
    std::string path_;
    std::FILE* f_ = nullptr;
    std::mutex mu_;
    uint64_t nextLsn_ = 1;
};

} // namespace minidb
