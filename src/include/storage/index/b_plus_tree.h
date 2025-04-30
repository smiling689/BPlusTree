/**
 * b_plus_tree.h
 *
 * Implementation of simple b+ tree data structure where internal pages direct
 * the search and leaf pages contain actual data.
 * (1) We only support unique key
 * (2) support insert & remove
 * (3) The structure should shrink and grow dynamically
 * (4) Implement index iterator for range scan
 */
#pragma once

#include <algorithm>
#include <deque>
#include <iostream>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <vector>

#include "common/config.h"
#include "common/macros.h"
#include "concurrency/transaction.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_header_page.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/page/page_guard.h"

namespace bustub {
    struct PrintableBPlusTree;

    /**
     * @brief Definition of the Context class.
     *
     * Hint: This class is designed to help you keep track of the pages
     * that you're modifying or accessing.
     *
     * Context 类可用于编写 B+ 树的螃蟹法则。 你可以使用它存储一条链上的锁， 也可以自己实现一个数据结构实现螃蟹法则。
     */
    class Context {
    public:
        // When you insert into / remove from the B+ tree, store the write guard of header page here.
        // Remember to drop the header page guard and set it to nullopt when you want to unlock all.
        std::optional<WritePageGuard> header_page_{std::nullopt};

        // Save the root page id here so that it's easier to know if the current page is the root page.
        page_id_t root_page_id_{INVALID_PAGE_ID};

        // Store the write guards of the pages that you're modifying here.
        std::deque<WritePageGuard> write_set_;

        // You may want to use this when getting value, but not necessary.
        std::deque<ReadPageGuard> read_set_;

        auto IsRootPage(page_id_t page_id) -> bool { return page_id == root_page_id_; }

        void clear() {
            header_page_ = std::nullopt;
            while (!write_set_.empty()) {
                write_set_.pop_front();
            }
            while (!read_set_.empty()) {
                read_set_.pop_front();
            }
        }
    };

#define BPLUSTREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

    // Main class providing the API for the Interactive B+ Tree.
    INDEX_TEMPLATE_ARGUMENTS
    class BPlusTree {
        using InternalPage = BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>;
        using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;

    public:
        explicit BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                           const KeyComparator &comparator, int leaf_max_size = LEAF_PAGE_SIZE,
                           int internal_max_size = INTERNAL_PAGE_SIZE);

        // Returns true if this B+ tree has no keys and values.
        auto IsEmpty() const -> bool;

        // Insert a key-value pair into this B+ tree.
        auto Insert(const KeyType &key, const ValueType &value, Transaction *txn = nullptr) -> bool;

        //回插
        void Insert_Up(const KeyType &key, page_id_t right_child, Context &path);

        //看是否安全，用于context
        bool Safe_Insert(const BPlusTreePage *tree_page);

        // Remove a key and its value from this B+ tree.
        void Remove(const KeyType &key, Transaction *txn);

        //回删
        void Remove_Up(int pos, Context &path);

        //看是否安全，用于context
        bool Safe_Remove(const BPlusTreePage *tree_page , bool root);

        // Return the value associated with a given key
        auto GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *txn = nullptr) -> bool;

        // Return the page id of the root node
        auto GetRootPageId() -> page_id_t;

        // Index iterator
        auto Begin() -> INDEXITERATOR_TYPE;

        auto End() -> INDEXITERATOR_TYPE;

        auto Begin(const KeyType &key) -> INDEXITERATOR_TYPE;

        //Binary find in page
        auto BinaryFind(const LeafPage *leaf_page, const KeyType &key) -> int;

        auto BinaryFind(const InternalPage *internal_page, const KeyType &key) -> int;

        // Print the B+ tree
        void Print(BufferPoolManager *bpm);

        // Draw the B+ tree
        void Draw(BufferPoolManager *bpm, const std::string &outf);

        /**
         * @brief draw a B+ tree, below is a printed
         * B+ tree(3 max leaf, 4 max internal) after inserting key:
         *  {1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 18, 19, 20}
         *
         *                               (25)
         *                 (9,17,19)                          (33)
         *  (1,5)    (9,13)    (17,18)    (19,20,21)    (25,29)    (33,37)
         *
         * @return std::string
         */
        auto DrawBPlusTree() -> std::string;

        // read data from file and insert one by one
        void InsertFromFile(const std::string &file_name, Transaction *txn = nullptr);

        // read data from file and remove one by one
        void RemoveFromFile(const std::string &file_name, Transaction *txn = nullptr);

        /**
         * @brief Read batch operations from input file, below is a sample file format
         * insert some keys and delete 8, 9 from the tree with one step.
         * { i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i30 d8 d9 } //  batch.txt
         * B+ Tree(4 max leaf, 4 max internal) after processing:
         *                            (5)
         *                 (3)                (7)
         *            (1,2)    (3,4)    (5,6)    (7,10,30) //  The output tree example
         */
        void BatchOpsFromFile(const std::string &file_name, Transaction *txn = nullptr);



    private:
        /* Debug Routines for FREE!! */
        void ToGraph(page_id_t page_id, const BPlusTreePage *page, std::ofstream &out);

        void PrintTree(page_id_t page_id, const BPlusTreePage *page);

        /**
         * @brief Convert A B+ tree into a Printable B+ tree
         *
         * @param root_id
         * @return PrintableNode
         */
        auto ToPrintableBPlusTree(page_id_t root_id) -> PrintableBPlusTree;

        // member variable
        std::string index_name_;//索引名称
        BufferPoolManager *bpm_;//指向 BufferPoolManager 的指针，用于管理页面的缓存。
        KeyComparator comparator_;//键的比较器，用于比较键的大小。
        std::vector<std::string> log; // NOLINT   日志向量，可能用于记录操作信息。
        int leaf_max_size_;//叶子节点的最大键数量
        int internal_max_size_;//内部节点的最大键数量。
        page_id_t header_page_id_;//头页面的页面 ID
    };

    /**
     * @brief for test only. PrintableBPlusTree is a printable B+ tree.
     * We first convert B+ tree into a printable B+ tree and the print it.
     */
    struct PrintableBPlusTree {
        int size_;
        std::string keys_;
        std::vector<PrintableBPlusTree> children_;

        /**
         * @brief BFS traverse a printable B+ tree and print it into
         * into out_buf
         *
         * @param out_buf
         */
        void Print(std::ostream &out_buf) {
            std::vector<PrintableBPlusTree *> que = {this};
            while (!que.empty()) {
                std::vector<PrintableBPlusTree *> new_que;

                for (auto &t: que) {
                    int padding = (t->size_ - t->keys_.size()) / 2;
                    out_buf << std::string(padding, ' ');
                    out_buf << t->keys_;
                    out_buf << std::string(padding, ' ');

                    for (auto &c: t->children_) {
                        new_que.push_back(&c);
                    }
                }
                out_buf << "\n";
                que = new_que;
            }
        }
    };
} // namespace bustub
