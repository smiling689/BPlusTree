//===----------------------------------------------------------------------===//
//
//                         DB Project 
//                        
//
// Identification: src/include/page/b_plus_tree_page.h
//
// 
//
//===----------------------------------------------------------------------===//
#pragma once

#include <cassert>
#include <climits>
#include <cstdlib>
#include <string>

#include "buffer/buffer_pool_manager.h"
#include "storage/index/generic_key.h"

namespace bustub {
#define MappingType std::pair<KeyType, ValueType>

#define INDEX_TEMPLATE_ARGUMENTS \
  template <typename KeyType, typename ValueType, typename KeyComparator>

    // define page type enum
    enum class IndexPageType {
        INVALID_INDEX_PAGE = 0,
        LEAF_PAGE,
        INTERNAL_PAGE
    };

    /**
     * Both internal and leaf page are inherited from this page.
     *
     * It actually serves as a header part for each B+ tree page and
     * contains information shared by both leaf page and internal page.
     *
     * Header format (size in byte, 12 bytes in total):
     * ---------------------------------------------------------
     * | PageType (4) | CurrentSize (4) | MaxSize (4) |  ...   |
     * ---------------------------------------------------------
     * 我们的 B+ 树的 page 存储在上方原始 page 的 data 区域。
     * 你可以认为， 上方的 page 包裹着这里的 b_plus_tree_page。
     *
     * 在该类中， GetSize 用于得到该 b_plus_tree_page 当前存储的元素个数，
     * SetSize 用于设置该 b_plus_tree_page 的元素个数， IncreaseSize 用于增加其元素个数。
     * 此外， GetMaxSize 可以得到该 b_plus_tree_page 允许存储的最大元素个数，
     * GetMinSize 可以得到该 b_plus_tree_page 允许存储的最小元素个数。
     * 这些成员函数会在插入和删除操作时派上用场。
     *
     * 此外， IsLeafPage 成员函数可以返回该 Page 是否为继承类 BPlusTreeLeafPage。
     */

    class BPlusTreePage {
    public:
        // Delete all constructor / destructor to ensure memory safety
        BPlusTreePage() = delete;

        BPlusTreePage(const BPlusTreePage &other) = delete;

        ~BPlusTreePage() = delete;

        auto IsLeafPage() const -> bool;

        void SetPageType(IndexPageType page_type);

        auto GetSize() const -> int;

        void SetSize(int size);

        void IncreaseSize(int amount);

        auto GetMaxSize() const -> int;

        void SetMaxSize(int max_size);

        auto GetMinSize() const -> int;

    private:
        // Member variables, attributes that both internal and leaf page share
        IndexPageType page_type_;
        int size_;
        int max_size_;
    };
} // namespace bustub
