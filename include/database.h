#pragma once
#include "bptree.h"
#include "lock_manager.h"
#include "wal.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace minidb {

struct UndoEntry {
    std::string table;
    int64_t key = 0;
    std::optional<Row> before;
    std::optional<Row> after;
};

struct TxnState {
    uint64_t ts = 0;
    std::set<LockKey> locks;
    std::vector<UndoEntry> undo;
};

class Database {
public:
    explicit Database(const std::string& walPath);

    void createTable(const std::string& name);
    uint64_t begin();
    bool insert(uint64_t txn, const std::string& table, int64_t key, const Row& row);
    bool update(uint64_t txn, const std::string& table, int64_t key, const Row& row);
    bool erase(uint64_t txn, const std::string& table, int64_t key);
    bool get(uint64_t txn, const std::string& table, int64_t key, Row& out);
    std::vector<std::pair<int64_t, Row>> range(uint64_t txn, const std::string& table, int64_t lo, int64_t hi);
    void commit(uint64_t txn);
    void abort(uint64_t txn);
    void recover();

    bool hasTable(const std::string& name) const;
    const BPlusTree* tableView(const std::string& name) const;

private:
    BPlusTree& tableOrThrow(const std::string& name);
    const BPlusTree& tableOrThrow(const std::string& name) const;

    bool lock(uint64_t txn, const std::string& table, int64_t key, LockMode mode);
    TxnState& txnOrThrow(uint64_t txn);

    std::string walPath_;
    WAL wal_;
    LockManager locks_;
    std::map<std::string, BPlusTree> tables_;
    std::map<uint64_t, TxnState> txns_;
    uint64_t nextTxn_ = 1;
    uint64_t nextTs_ = 1;
};

} // namespace minidb
