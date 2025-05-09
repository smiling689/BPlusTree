//===----------------------------------------------------------------------===//
//
//                         DB Project 
//                        
//
// Identification: src/page/b_plus_tree_leaf_page.cpp
//
// 
//
//===----------------------------------------------------------------------===//

#include "storage/page/b_plus_tree_leaf_page.h"

#include <sstream>

#include "common/exception.h"
#include "common/rid.h"

namespace bustub {
    /*****************************************************************************
     * HELPER METHODS AND UTILITIES
     *****************************************************************************/

    /**
     * Init method after creating a new leaf page
     * Including set page type, set current size to zero, set next page id and set
     * max size
     */

    INDEX_TEMPLATE_ARGUMENTS
    void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
        SetPageType(IndexPageType::LEAF_PAGE);
        SetSize(0);
        SetMaxSize(max_size);
        SetNextPageId(INVALID_PAGE_ID);
    }

    /**
     * Helper methods to set/get next page id
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t {
        return next_page_id_;
    }

    INDEX_TEMPLATE_ARGUMENTS
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) {
        this->next_page_id_ = next_page_id;
    }

    /*
     * Helper method to find and return the key associated with input "index"(a.k.a
     * array offset)
     */

    INDEX_TEMPLATE_ARGUMENTS
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetAt(int index, const KeyType &key,
                                           const ValueType &value) {
        if (index >= 0 && index < GetSize()) {
            array_[index] = {key, value};
            return;
        }
    }

    INDEX_TEMPLATE_ARGUMENTS
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
        array_[index].first = key;
    }

    INDEX_TEMPLATE_ARGUMENTS
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetValueAt(int index, const ValueType &value) {
        array_[index].second = value;
    }


    INDEX_TEMPLATE_ARGUMENTS
    auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
        return array_[index].first;
    }

    INDEX_TEMPLATE_ARGUMENTS
    auto B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const -> ValueType {
        return array_[index].second;
    }

    template class BPlusTreeLeafPage<GenericKey<4>, RID, GenericComparator<4> >;
    template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8> >;
    template class BPlusTreeLeafPage<GenericKey<16>, RID, GenericComparator<16> >;
    template class BPlusTreeLeafPage<GenericKey<32>, RID, GenericComparator<32> >;
    template class BPlusTreeLeafPage<GenericKey<64>, RID, GenericComparator<64> >;
} // namespace bustub
