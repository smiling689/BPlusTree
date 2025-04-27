//===----------------------------------------------------------------------===//
//
//                         DB Project 
//                        
//
// Identification: src/include/page/b_plus_tree_internal_page.h
//
// 
//
//===----------------------------------------------------------------------===//
#pragma once

#include <queue>
#include <string>

#include "storage/page/b_plus_tree_page.h"

namespace bustub {
#define B_PLUS_TREE_INTERNAL_PAGE_TYPE \
  BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>
#define INTERNAL_PAGE_HEADER_SIZE 12
#define INTERNAL_PAGE_SIZE \
  ((BUSTUB_PAGE_SIZE - INTERNAL_PAGE_HEADER_SIZE) / (sizeof(MappingType)))

    /**
     * Store `n` indexed keys and `n + 1` child pointers (page_id) within internal
     * page. Pointer PAGE_ID(i) points to a subtree in which all keys K satisfy:
     * K(i) <= K < K(i+1).
     * NOTE: Since the number of keys does not equal to number of child pointers,
     * the first key always remains invalid. That is to say, any search / lookup
     * should ignore the first key.
     *
     * Internal page format (keys are stored in increasing order):
     * ----------------------------------------------------------------------------------
     * | HEADER | KEY(1) + PAGE_ID(1) | KEY(2) + PAGE_ID(2) | ... | KEY(n) +
     * PAGE_ID(n) |
     * ----------------------------------------------------------------------------------
     *
     * Internal page 对应 B+ 树的内部结点， leaf page 对应 B+ 树的叶子结点。
     * 对于 internal page,
     * 它存储着 n 个 索引 key 和 n + 1 个指向 children page 的指针。
     * (由于我们以数组形式存储， 因此第一个数组元素对应的索引项无实际意义)
     *
     *Init 函数可用于手动刷新这个 b_plus_tree_page，
     *通常你不会手动调用这个成员函数， 但如果你的实现需要用到刷新 b_plus_tree_page, 你可以考虑调用它。
     *
     *另外请注意， 这两个类继承自 BPlusTreePage，
     *因此别忘了可以使用 BPlusTreePage 的成员函数 (如 GetSize, IncreaseSize)！
     *
     */

    INDEX_TEMPLATE_ARGUMENTS
    // 这是为了写模板简便定义的一个宏：
    // #define INDEX_TEMPLATE_ARGUMENTS template <typename KeyType, typename ValueType, typename KeyComparator>
    class BPlusTreeInternalPage : public BPlusTreePage {
    public:
        // Delete all constructor / destructor to ensure memory safety
        BPlusTreeInternalPage() = delete;

        BPlusTreeInternalPage(const BPlusTreeInternalPage &other) = delete;

        /**
         * Writes the necessary header information to a newly created page, must be
         * called after the creation of a new page to make a valid
         * `BPlusTreeInternalPage`
         * @param max_size Maximal size of the page
         */
        void Init(int max_size = INTERNAL_PAGE_SIZE);

        /**
         * @param index The index of the key to get. Index must be non-zero.
         * @return Key at index
         */
        auto KeyAt(int index) const -> KeyType;

        /**
         * @param index The index of the key to set. Index must be non-zero.
         * @param key The new value for key
         */
        void SetKeyAt(int index, const KeyType &key);

        /**
         * @param value The value to search for
         * @return The index that corresponds to the specified value
         */
        auto ValueIndex(const ValueType &value) const -> int;

        /**
         * @param index The index to search for
         * @return The value at the index
         */
        auto ValueAt(int index) const -> ValueType;

        /**
         * @brief For test only, return a string representing all keys in
         * this internal page, formatted as "(key1,key2,key3,...)"
         *
         * @return The string representation of all keys in the current internal page
         */
        void SetValueAt(int index, const ValueType &value);

        auto ToString() const -> std::string {
            std::string kstr = "(";
            bool first = true;

            // First key of internal page is always invalid
            for (int i = 1; i < GetSize(); i++) {
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
        // Flexible array member for page data.
        MappingType array_[0];
        //MappingType 是这样定义的一个宏： #define MappingType std::pair<KeyType, ValueType>
    };
} // namespace bustub
