#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace minidb {

struct Row {
    std::string name;
    int64_t value = 0;
};

inline bool operator==(const Row& a, const Row& b) {
    return a.name == b.name && a.value == b.value;
}

class TxnAbort : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace minidb
