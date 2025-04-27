//===----------------------------------------------------------------------===//
//
//                         DB Project 
//                        
//
// Identification: src/include/page/b_plus_tree_leaf_page.h
//
// 
//
//===----------------------------------------------------------------------===//
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "storage/page/b_plus_tree_page.h"

namespace bustub {
#define B_PLUS_TREE_LEAF_PAGE_TYPE \
  BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>
#define LEAF_PAGE_HEADER_SIZE 16
#define LEAF_PAGE_SIZE \
  ((BUSTUB_PAGE_SIZE - LEAF_PAGE_HEADER_SIZE) / sizeof(MappingType))

    /**
     * Store indexed key and record id (record id = page id combined with slot id,
     * see `include/common/rid.h` for detailed implementation) together within leaf
     * page. Only support unique key.
     *
     * Leaf page format (keys are stored in order):
     * -----------------------------------------------------------------------
     * | HEADER | KEY(1) + RID(1) | KEY(2) + RID(2) | ... | KEY(n) + RID(n)  |
     * -----------------------------------------------------------------------
     *
     * Header format (size in byte, 16 bytes in total):
     * -----------------------------------------------------------------------
     * | PageType (4) | CurrentSize (4) | MaxSize (4) | NextPageId (4) | ... |
     * -----------------------------------------------------------------------
     *
     * 对于 LeafPage, 它存储着 n 个 索引 key 和 n 个对应的数据行 ID。
     * 这里的 "KeyAt", "SetKeyAt", "ValueAt", "SetValueAt" 可用于键值对的查询与更新，
     * 会在 B+ 树的编写中用到。所有的叶子节点形成一个链表，
     * 辅助函数 GetNextPageId 和 SetNextPageId 可用于维护这个链表。
     *
     * 另外请注意， 这两个类继承自 BPlusTreePage，
     * 因此别忘了可以使用 BPlusTreePage 的成员函数 (如 GetSize, IncreaseSize)！
     *
     */
    INDEX_TEMPLATE_ARGUMENTS
    // 这是为了写模板简便定义的一个宏：
    // #define INDEX_TEMPLATE_ARGUMENTS template <typename KeyType, typename ValueType, typename KeyComparator>
    class BPlusTreeLeafPage : public BPlusTreePage {
    public:
        // Delete all constructor / destructor to ensure memory safety
        BPlusTreeLeafPage() = delete;

        BPlusTreeLeafPage(const BPlusTreeLeafPage &other) = delete;

        /**
         * After creating a new leaf page from buffer pool, must call initialize
         * method to set default values
         * @param max_size Max size of the leaf node
         */
        void Init(int max_size = LEAF_PAGE_SIZE);

        // Helper methods
        auto GetNextPageId() const -> page_id_t;

        void SetNextPageId(page_id_t next_page_id);

        auto KeyAt(int index) const -> KeyType;

        auto ValueAt(int index) const -> ValueType;

        void SetAt(int index, const KeyType &key, const ValueType &value);

        void SetKeyAt(int index, const KeyType &key);

        void SetValueAt(int index, const ValueType &value);

        /**
         * @brief For test only return a string representing all keys in
         * this leaf page formatted as "(key1,key2,key3,...)"
         *
         * @return The string representation of all keys in the current internal page
         */
        auto ToString() const -> std::string {
            std::string kstr = "(";
            bool first = true;

            for (int i = 0; i < GetSize(); i++) {
                KeyType key = KeyAt(i);
                if (first) {
                    first = false;
                } else {
                    kstr.append(",");
                }

                kstr.append(std::to_string(key.ToString()));
            }
            kstr.append(")");

            return kstr;
        }

    private:
        page_id_t next_page_id_;
        // Flexible array member for page data.
        MappingType array_[0];
        //MappingType 是这样定义的一个宏： #define MappingType std::pair<KeyType, ValueType>
    };
} // namespace bustub
