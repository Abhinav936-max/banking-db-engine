#include "database.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace minidb {

Database::Database(const std::string& walPath)
    : walPath_(walPath), wal_(walPath, false) {
    recover();
}

void Database::createTable(const std::string& name) {
    if (tables_.count(name)) return;
    tables_.emplace(name, BPlusTree{});
    LogRecord rec{}; rec.type = LogRecord::Type::Create; rec.table = name; wal_.append(rec, true);
}

uint64_t Database::begin() {
    uint64_t txn = nextTxn_++;
    txns_[txn] = TxnState{nextTs_++, {}, {}};
    LogRecord rec{}; rec.type = LogRecord::Type::Begin; rec.txn = txn; wal_.append(rec, false);
    return txn;
}

bool Database::hasTable(const std::string& name) const {
    return tables_.count(name) != 0;
}

const BPlusTree* Database::tableView(const std::string& name) const {
    auto it = tables_.find(name);
    return it == tables_.end() ? nullptr : &it->second;
}

BPlusTree& Database::tableOrThrow(const std::string& name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) throw std::runtime_error("unknown table: " + name);
    return it->second;
}

const BPlusTree& Database::tableOrThrow(const std::string& name) const {
    auto it = tables_.find(name);
    if (it == tables_.end()) throw std::runtime_error("unknown table: " + name);
    return it->second;
}

TxnState& Database::txnOrThrow(uint64_t txn) {
    auto it = txns_.find(txn);
    if (it == txns_.end()) throw std::runtime_error("unknown txn");
    return it->second;
}

bool Database::lock(uint64_t txn, const std::string& table, int64_t key, LockMode mode) {
    auto& t = txnOrThrow(txn);
    LockKey lk{table, key};
    if (locks_.lock(txn, t.ts, lk, mode)) {
        t.locks.insert(lk);
        return true;
    }
    abort(txn);
    return false;
}

bool Database::get(uint64_t txn, const std::string& table, int64_t key, Row& out) {
    if (!lock(txn, table, key, LockMode::Shared)) return false;
    return tableOrThrow(table).get(key, out);
}

bool Database::insert(uint64_t txn, const std::string& table, int64_t key, const Row& row) {
    if (!lock(txn, table, key, LockMode::Exclusive)) return false;
    auto& tree = tableOrThrow(table);
    Row before;
    std::optional<Row> old;
    if (tree.get(key, before)) old = before;
    if (!tree.insert(key, row)) return false;
    txnOrThrow(txn).undo.push_back({table, key, old, row});
    LogRecord rec{}; rec.type = LogRecord::Type::Change; rec.txn = txn; rec.table = table; rec.key = key; rec.before = old; rec.after = row; wal_.append(rec, false);
    return true;
}

bool Database::update(uint64_t txn, const std::string& table, int64_t key, const Row& row) {
    if (!lock(txn, table, key, LockMode::Exclusive)) return false;
    auto& tree = tableOrThrow(table);
    Row before;
    if (!tree.get(key, before)) return false;
    tree.update(key, row);
    txnOrThrow(txn).undo.push_back({table, key, before, row});
    LogRecord rec{}; rec.type = LogRecord::Type::Change; rec.txn = txn; rec.table = table; rec.key = key; rec.before = before; rec.after = row; wal_.append(rec, false);
    return true;
}

bool Database::erase(uint64_t txn, const std::string& table, int64_t key) {
    if (!lock(txn, table, key, LockMode::Exclusive)) return false;
    auto& tree = tableOrThrow(table);
    Row before;
    if (!tree.get(key, before)) return false;
    tree.erase(key);
    txnOrThrow(txn).undo.push_back({table, key, before, std::nullopt});
    LogRecord rec{}; rec.type = LogRecord::Type::Change; rec.txn = txn; rec.table = table; rec.key = key; rec.before = before; rec.after = std::nullopt; wal_.append(rec, false);
    return true;
}

std::vector<std::pair<int64_t, Row>> Database::range(uint64_t txn, const std::string& table, int64_t lo, int64_t hi) {
    (void)txn;
    return tableOrThrow(table).range(lo, hi);
}

void Database::commit(uint64_t txn) {
    auto it = txns_.find(txn);
    if (it == txns_.end()) return;
    LogRecord rec{}; rec.type = LogRecord::Type::Commit; rec.txn = txn; wal_.append(rec, true);
    locks_.releaseAll(txn, it->second.locks);
    txns_.erase(it);
}

void Database::abort(uint64_t txn) {
    auto it = txns_.find(txn);
    if (it == txns_.end()) return;
    for (auto rit = it->second.undo.rbegin(); rit != it->second.undo.rend(); ++rit) {
        auto& tree = tableOrThrow(rit->table);
        if (rit->before) tree.update(rit->key, *rit->before);
        else tree.erase(rit->key);
    }
    LogRecord rec{}; rec.type = LogRecord::Type::Abort; rec.txn = txn; wal_.append(rec, true);
    locks_.releaseAll(txn, it->second.locks);
    txns_.erase(it);
}

void Database::recover() {
    auto records = WAL::readAll(walPath_);
    std::set<std::string> created;
    std::set<uint64_t> committed;
    for (const auto& r : records) {
        if (r.type == LogRecord::Type::Create) created.insert(r.table);
        if (r.type == LogRecord::Type::Commit) committed.insert(r.txn);
    }
    for (const auto& t : created) tables_.emplace(t, BPlusTree{});
    for (const auto& r : records) {
        if (r.type == LogRecord::Type::Change && committed.count(r.txn)) {
            auto& tree = tables_[r.table];
            if (r.after) tree.update(r.key, *r.after) || tree.insert(r.key, *r.after);
            else tree.erase(r.key);
        }
    }
}

} // namespace minidb
