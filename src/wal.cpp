#include "wal.h"

#include <fstream>
#include <sstream>
#include <unistd.h>

namespace minidb {

namespace {
std::string encodeRow(const std::optional<Row>& r) {
    if (!r) return "0";
    return "1 " + r->name + " " + std::to_string(r->value);
}

std::string encode(const LogRecord& r) {
    std::ostringstream o;
    o << r.lsn << ' ';
    switch (r.type) {
        case LogRecord::Type::Create: o << "CREATE " << r.table; break;
        case LogRecord::Type::Begin: o << "BEGIN " << r.txn; break;
        case LogRecord::Type::Commit: o << "COMMIT " << r.txn; break;
        case LogRecord::Type::Abort: o << "ABORT " << r.txn; break;
        case LogRecord::Type::Change:
            o << "CHANGE " << r.txn << ' ' << r.table << ' ' << r.key << ' ' << encodeRow(r.before) << ' '
              << encodeRow(r.after);
            break;
    }
    o << '\n';
    return o.str();
}

bool readImage(std::istringstream& in, std::optional<Row>& out) {
    int has = 0;
    if (!(in >> has)) return false;
    if (has) {
        Row r;
        if (!(in >> r.name >> r.value)) return false;
        out = r;
    }
    return true;
}

bool decode(const std::string& line, LogRecord& r) {
    std::istringstream in(line);
    std::string word;
    if (!(in >> r.lsn >> word)) return false;
    if (word == "CREATE") {
        r.type = LogRecord::Type::Create;
        return static_cast<bool>(in >> r.table);
    }
    if (word == "BEGIN" || word == "COMMIT" || word == "ABORT") {
        r.type = word == "BEGIN" ? LogRecord::Type::Begin
                                 : (word == "COMMIT" ? LogRecord::Type::Commit : LogRecord::Type::Abort);
        return static_cast<bool>(in >> r.txn);
    }
    if (word == "CHANGE") {
        r.type = LogRecord::Type::Change;
        if (!(in >> r.txn >> r.table >> r.key)) return false;
        return readImage(in, r.before) && readImage(in, r.after);
    }
    return false;
}
}  // namespace

WAL::WAL(const std::string& path, bool truncate) : path_(path) {
    f_ = std::fopen(path.c_str(), truncate ? "w" : "a");
    if (!f_) throw std::runtime_error("cannot open WAL: " + path);
}

WAL::~WAL() {
    if (f_) std::fclose(f_);
}

uint64_t WAL::append(LogRecord rec, bool force) {
    std::lock_guard<std::mutex> lk(mu_);
    rec.lsn = nextLsn_++;
    std::string line = encode(rec);
    std::fputs(line.c_str(), f_);
    std::fflush(f_);
    if (force) ::fsync(::fileno(f_));
    return rec.lsn;
}

void WAL::reset() {
    std::lock_guard<std::mutex> lk(mu_);
    std::fclose(f_);
    f_ = std::fopen(path_.c_str(), "w");
    if (!f_) throw std::runtime_error("cannot reset WAL");
    ::fsync(::fileno(f_));
}

std::vector<LogRecord> WAL::readAll(const std::string& path) {
    std::vector<LogRecord> out;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (in.eof()) break;
        LogRecord r;
        if (!decode(line, r)) break;
        out.push_back(std::move(r));
    }
    return out;
}

} // namespace minidb
