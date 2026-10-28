#pragma once
#include "common.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace minidb {

enum class LockMode { Shared, Exclusive };
using LockKey = std::pair<std::string, int64_t>;

class LockManager {
public:
    bool lock(uint64_t txn, uint64_t ts, const LockKey& key, LockMode mode) {
        std::unique_lock<std::mutex> lk(mu_);
        for (;;) {
            LockState& st = table_[key];
            if (st.xHolder == txn) return true;
            if (mode == LockMode::Shared && st.shared.count(txn)) return true;

            std::vector<uint64_t> holderTs;
            if (st.xHolder) holderTs.push_back(st.xTs);
            if (mode == LockMode::Exclusive) {
                for (const auto& [t, tts] : st.shared)
                    if (t != txn) holderTs.push_back(tts);
            } else {
                for (const auto& [t, tts] : st.shared)
                    if (t != txn && tts != ts) holderTs.push_back(tts);
            }

            if (holderTs.empty()) {
                if (mode == LockMode::Shared) st.shared[txn] = ts;
                else {
                    st.shared.erase(txn);
                    st.xHolder = txn;
                    st.xTs = ts;
                }
                return true;
            }

            for (uint64_t h : holderTs) if (ts > h) return false;
            cv_.wait(lk);
        }
    }

    void releaseAll(uint64_t txn, const std::set<LockKey>& keys) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            for (const auto& k : keys) {
                auto it = table_.find(k);
                if (it == table_.end()) continue;
                it->second.shared.erase(txn);
                if (it->second.xHolder == txn) it->second.xHolder = 0, it->second.xTs = 0;
                if (it->second.shared.empty() && it->second.xHolder == 0) table_.erase(it);
            }
        }
        cv_.notify_all();
    }

private:
    struct LockState {
        std::map<uint64_t, uint64_t> shared;
        uint64_t xHolder = 0;
        uint64_t xTs = 0;
    };

    std::mutex mu_;
    std::condition_variable cv_;
    std::map<LockKey, LockState> table_;
};

} // namespace minidb
