#pragma once
#include "common.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace minidb {

constexpr int kBlockSize = 4096;
constexpr int kDefaultMaxKeys = (kBlockSize - 16) / static_cast<int>(sizeof(int64_t) + sizeof(int64_t));

class BPlusTree {
public:
    explicit BPlusTree(int maxKeys = kDefaultMaxKeys)
        : maxKeys_(maxKeys < 3 ? 3 : maxKeys), root_(std::make_unique<Node>(true)) {}

    bool insert(int64_t key, const Row& row) {
        bool inserted = false;
        Split s = insertRec(root_.get(), key, row, inserted);
        if (s.split) {
            auto newRoot = std::make_unique<Node>(false);
            newRoot->keys.push_back(s.key);
            newRoot->children.push_back(std::move(root_));
            newRoot->children.push_back(std::move(s.right));
            root_ = std::move(newRoot);
        }
        if (inserted) ++size_;
        return inserted;
    }

    bool get(int64_t key, Row& out) const {
        const Node* leaf = findLeaf(key);
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it == leaf->keys.end() || *it != key) return false;
        out = leaf->rows[it - leaf->keys.begin()];
        return true;
    }

    bool update(int64_t key, const Row& row) {
        Node* leaf = findLeaf(key);
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it == leaf->keys.end() || *it != key) return false;
        leaf->rows[it - leaf->keys.begin()] = row;
        return true;
    }

    bool erase(int64_t key) {
        Node* leaf = findLeaf(key);
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it == leaf->keys.end() || *it != key) return false;
        size_t idx = it - leaf->keys.begin();
        leaf->keys.erase(leaf->keys.begin() + idx);
        leaf->rows.erase(leaf->rows.begin() + idx);
        --size_;
        return true;
    }

    std::vector<std::pair<int64_t, Row>> range(int64_t lo, int64_t hi) const {
        std::vector<std::pair<int64_t, Row>> out;
        for (const Node* leaf = findLeaf(lo); leaf; leaf = leaf->next) {
            for (size_t i = 0; i < leaf->keys.size(); ++i) {
                if (leaf->keys[i] > hi) return out;
                if (leaf->keys[i] >= lo) out.emplace_back(leaf->keys[i], leaf->rows[i]);
            }
        }
        return out;
    }

    size_t size() const { return size_; }
    int maxKeys() const { return maxKeys_; }

    int height() const {
        int h = 1;
        for (const Node* n = root_.get(); !n->isLeaf; n = n->children[0].get()) ++h;
        return h;
    }

    bool validate() const {
        if (!checkNode(root_.get(), nullptr, nullptr)) return false;
        size_t count = 0;
        const Node* n = root_.get();
        while (!n->isLeaf) n = n->children[0].get();
        bool havePrev = false;
        int64_t prev = 0;
        for (; n; n = n->next) {
            for (int64_t k : n->keys) {
                if (havePrev && k <= prev) return false;
                prev = k;
                havePrev = true;
                ++count;
            }
        }
        return count == size_;
    }

private:
    struct Node {
        explicit Node(bool leaf) : isLeaf(leaf) {}
        bool isLeaf;
        std::vector<int64_t> keys;
        std::vector<Row> rows;
        std::vector<std::unique_ptr<Node>> children;
        Node* next = nullptr;
    };

    struct Split {
        bool split = false;
        int64_t key = 0;
        std::unique_ptr<Node> right;
    };

    Node* findLeaf(int64_t key) const {
        Node* n = root_.get();
        while (!n->isLeaf) {
            size_t i = std::upper_bound(n->keys.begin(), n->keys.end(), key) - n->keys.begin();
            n = n->children[i].get();
        }
        return n;
    }

    Split insertRec(Node* n, int64_t key, const Row& row, bool& inserted) {
        if (n->isLeaf) {
            size_t i = std::lower_bound(n->keys.begin(), n->keys.end(), key) - n->keys.begin();
            if (i < n->keys.size() && n->keys[i] == key) return {};
            n->keys.insert(n->keys.begin() + i, key);
            n->rows.insert(n->rows.begin() + i, row);
            inserted = true;
            if (static_cast<int>(n->keys.size()) <= maxKeys_) return {};
            size_t mid = n->keys.size() / 2;
            auto right = std::make_unique<Node>(true);
            right->keys.assign(n->keys.begin() + mid, n->keys.end());
            right->rows.assign(std::make_move_iterator(n->rows.begin() + mid), std::make_move_iterator(n->rows.end()));
            n->keys.resize(mid);
            n->rows.resize(mid);
            right->next = n->next;
            n->next = right.get();
            Split s;
            s.split = true;
            s.key = right->keys.front();
            s.right = std::move(right);
            return s;
        }

        size_t i = std::upper_bound(n->keys.begin(), n->keys.end(), key) - n->keys.begin();
        Split child = insertRec(n->children[i].get(), key, row, inserted);
        if (!child.split) return {};
        n->keys.insert(n->keys.begin() + i, child.key);
        n->children.insert(n->children.begin() + i + 1, std::move(child.right));
        if (static_cast<int>(n->keys.size()) <= maxKeys_) return {};
        size_t mid = n->keys.size() / 2;
        Split s;
        s.split = true;
        s.key = n->keys[mid];
        auto right = std::make_unique<Node>(false);
        right->keys.assign(n->keys.begin() + mid + 1, n->keys.end());
        for (size_t j = mid + 1; j < n->children.size(); ++j) right->children.push_back(std::move(n->children[j]));
        n->keys.resize(mid);
        n->children.resize(mid + 1);
        s.right = std::move(right);
        return s;
    }

    bool checkNode(const Node* n, const int64_t* lo, const int64_t* hi) const {
        for (size_t i = 0; i < n->keys.size(); ++i) {
            if (lo && n->keys[i] < *lo) return false;
            if (hi && n->keys[i] >= *hi) return false;
            if (i > 0 && n->keys[i] <= n->keys[i - 1]) return false;
        }
        if (n->isLeaf) return n->keys.size() == n->rows.size();
        if (n->children.size() != n->keys.size() + 1) return false;
        for (size_t i = 0; i < n->children.size(); ++i) {
            const int64_t* l = (i == 0) ? lo : &n->keys[i - 1];
            const int64_t* h = (i == n->keys.size()) ? hi : &n->keys[i];
            if (!checkNode(n->children[i].get(), l, h)) return false;
        }
        return true;
    }

    int maxKeys_;
    std::unique_ptr<Node> root_;
    size_t size_ = 0;
};

} // namespace minidb
