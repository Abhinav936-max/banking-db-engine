#include "database.h"
#include <iostream>

using namespace minidb;

int main() {
    try {
        Database db("wal.log");
        db.createTable("accounts");
        db.createTable("products");

        auto t1 = db.begin();
        db.insert(t1, "accounts", 1001, {"Alice", 5000});
        db.insert(t1, "accounts", 1002, {"Bob", 3200});
        db.insert(t1, "products", 501, {"Keyboard", 25});
        db.commit(t1);

        auto t2 = db.begin();
        db.update(t2, "accounts", 1002, {"Bob", 4000});
        Row out;
        db.get(t2, "accounts", 1002, out);
        std::cout << "account 1002 -> " << out.name << " " << out.value << "\n";
        db.commit(t2);

        auto t3 = db.begin();
        auto rows = db.range(t3, "accounts", 1000, 2000);
        for (auto& [k, r] : rows) {
            std::cout << k << " " << r.name << " " << r.value << "\n";
        }
        db.commit(t3);

        std::cout << "B+ tree height: " << db.tableView("accounts")->height() << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
