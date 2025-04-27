#include "storage/index/b_plus_tree.h"

#include <sstream>
#include <string>

#include "buffer/lru_k_replacer.h"
#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "common/rid.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_header_page.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/page/b_plus_tree_page.h"
#include "storage/page/page_guard.h"

namespace bustub {
    /**
     * 我们额外留心一下 B+ 树的构造函数：
     * 这里需要注意的是， 我们传入了一个比较函数的函数对象，
     * 如果你希望对 key 进行比较， 请使用这里的 comparator_ 函数对象。
     *
    * 如果你希望在测试时修改索引的值为某一整数， 你可以使用 SetFromInteger, 如

    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, txn);

     */
    INDEX_TEMPLATE_ARGUMENTS
    BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id,
                              BufferPoolManager *buffer_pool_manager,
                              const KeyComparator &comparator, int leaf_max_size,
                              int internal_max_size)
        : index_name_(std::move(name)),
          bpm_(buffer_pool_manager),
          comparator_(std::move(comparator)),
          leaf_max_size_(leaf_max_size),
          internal_max_size_(internal_max_size),
          header_page_id_(header_page_id) {
        WritePageGuard guard = bpm_->FetchPageWrite(header_page_id_);
        // In the original bpt, I fetch the header page
        // thus there's at least one page now
        auto root_header_page = guard.template AsMut<BPlusTreeHeaderPage>();
        // reinterprete the data of the page into "HeaderPage"
        root_header_page->root_page_id_ = INVALID_PAGE_ID;
        // set the root_id to INVALID
    }

    /**
     * Helper function to decide whether current b+tree is empty
     *
    *
    * 这里的 guard.template As<BPlusTreeHeaderPage>();
    * 即为获取我们读到的 ReadPageGuard 封装的 page 的 data 区域，
    * 将这块区域重新解释为 BPlusTreeHeaderPage 类型。
    * 也就是， 我们获取了一个用来读的 page， 然后把这个 page 里面的数据解释为 BPlusTreeHeaderPage，
    * 从而读取 header page 里的 root_page_id 信息， 检查是否是 INVALID.
    * 上方有一个很奇怪的函数，叫做 bpm_ -> FetchPageRead, 这是什么呢？ 请接着看。
    * 示例函数：
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
        ReadPageGuard guard = bpm_->FetchPageRead(header_page_id_);
        auto root_header_page = guard.template As<BPlusTreeHeaderPage>();
        bool is_empty = root_header_page->root_page_id_ == INVALID_PAGE_ID;
        // Just check if the root_page_id is INVALID
        // usage to fetch a page:
        // fetch the page guard   ->   call the "As" function of the page guard
        // to reinterprete the data of the page as "BPlusTreePage"
        return is_empty;
    }

    /*****************************************************************************
     * SEARCH
     *****************************************************************************/
    /*
     * Return the only value that associated with input key
     * This method is used for point query
     * @return : true means key exists
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::GetValue(const KeyType &key,
                                  std::vector<ValueType> *result, Transaction *txn)
        -> bool {
        auto header_page_guard = bpm_->FetchPageRead(header_page_id_);
        auto header_page = header_page_guard.template As<BPlusTreeHeaderPage>();
        if (header_page->root_page_id_ == INVALID_PAGE_ID) {
            return false;
        }
        page_id_t current_page_id = header_page->root_page_id_;

        while (true) {
            auto page_guard = bpm_->FetchPageRead(current_page_id);
            auto page = page_guard.template As<BPlusTreePage>();

            if (page->IsLeafPage()) {
                auto leaf_page = page_guard.template As<LeafPage>();
                int index = BinaryFind(leaf_page, key);
                if (index == -1 || comparator_(leaf_page->KeyAt(index), key) != 0) {
                    return false;
                }
                result->push_back(leaf_page->ValueAt(index));
                return true;
            }

            auto internal_page = page_guard.template As<InternalPage>();
            current_page_id = internal_page->ValueAt(BinaryFind(internal_page, key));
        }
    }


    /*****************************************************************************
     * INSERTION
     *****************************************************************************/
    /*
     * Insert constant key & value pair into b+ tree
     * if current tree is empty, start new tree, update root page id and insert
     * entry, otherwise insert into leaf page.
     * @return: since we only support unique key, if user try to insert duplicate
     * keys return false, otherwise return true.
     */


    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value,
                                Transaction *txn) -> bool {
    }

    auto IsSafe_Insert(BPlusTreePage *page, bool is_root) -> bool {
        if (page->IsLeafPage()) {
            return page->GetSize() + 1 < page->GetMaxSize();
        }
        return page->GetSize() < page->GetMaxSize();
    }

    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::FindLeafPage(const KeyType &key) {
        // 从 header page 获取 root page id
        ReadPageGuard header_page_guard = bpm_->FetchPageRead(header_page_id_);
        auto header_page = header_page_guard.template As<BPlusTreeHeaderPage>();
        page_id_t current_page_id = header_page->root_page_id_;
        header_page_guard.Drop();

        // 如果 root page id 无效，返回空的 ReadPageGuard
        if (current_page_id == INVALID_PAGE_ID) {
            return ReadPageGuard();
        }

        // 遍历树直到找到叶子节点
        while (true) {
            ReadPageGuard page_guard = bpm_->FetchPageRead(current_page_id);
            auto page = page_guard.template As<BPlusTreePage>();

            if (page->IsLeafPage()) {
                // 如果是叶子节点，返回该页面的 ReadPageGuard
                return page_guard;
            }

            // 如果是内部节点，找到合适的子节点页面 ID
            auto internal_page = page_guard.template As<InternalPage>();
            int index = BinaryFind(internal_page, key);
            current_page_id = internal_page->ValueAt(index);
        }
    }


    /*****************************************************************************
     * REMOVE
     *****************************************************************************/
    /*
     * Delete key & value pair associated with input key
     * If current tree is empty, return immediately.
     * If not, User needs to first find the right leaf page as deletion target, then
     * delete entry from leaf page. Remember to deal with redistribute or merge if
     * necessary.
     */

    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *txn) {
        //Your code here
    }

    /*****************************************************************************
     * INDEX ITERATOR
     *****************************************************************************/


    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::BinaryFind(const LeafPage *leaf_page, const KeyType &key)
        -> int {
        int l = 0;
        int r = leaf_page->GetSize() - 1;
        while (l < r) {
            int mid = (l + r + 1) >> 1;
            if (comparator_(leaf_page->KeyAt(mid), key) != 1) {
                l = mid;
            } else {
                r = mid - 1;
            }
        }

        if (r >= 0 && comparator_(leaf_page->KeyAt(r), key) == 1) {
            r = -1;
        }

        return r;
    }

    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::BinaryFind(const InternalPage *internal_page,
                                    const KeyType &key) -> int {
        int l = 1;
        int r = internal_page->GetSize() - 1;
        while (l < r) {
            int mid = (l + r + 1) >> 1;
            if (comparator_(internal_page->KeyAt(mid), key) != 1) {
                l = mid;
            } else {
                r = mid - 1;
            }
        }

        if (r == -1 || comparator_(internal_page->KeyAt(r), key) == 1) {
            r = 0;
        }

        return r;
    }

    /*
     * Input parameter is void, find the leftmost leaf page first, then construct
     * index iterator
     * @return : index iterator
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE
        //Just go left forever
    {
        ReadPageGuard head_guard = bpm_->FetchPageRead(header_page_id_);
        if (head_guard.template As<BPlusTreeHeaderPage>()->root_page_id_ == INVALID_PAGE_ID) {
            return End();
        }
        ReadPageGuard guard = bpm_->FetchPageRead(head_guard.As<BPlusTreeHeaderPage>()->root_page_id_);
        head_guard.Drop();

        auto tmp_page = guard.template As<BPlusTreePage>();
        while (!tmp_page->IsLeafPage()) {
            int slot_num = 0;
            guard = bpm_->FetchPageRead(reinterpret_cast<const InternalPage *>(tmp_page)->ValueAt(slot_num));
            tmp_page = guard.template As<BPlusTreePage>();
        }
        int slot_num = 0;
        if (slot_num != -1) {
            return INDEXITERATOR_TYPE(bpm_, guard.PageId(), 0);
        }
        return End();
    }


    /**
     * Input parameter is low key, find the leaf page that contains the input key
     * first, then construct index iterator
     * @return : index iterator
     * page使用例子
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
        ReadPageGuard head_guard = bpm_->FetchPageRead(header_page_id_);
        //读到 header page
        if (head_guard.template As<BPlusTreeHeaderPage>()->root_page_id_ == INVALID_PAGE_ID) {
            return End();
            //如果 header page 存的 root_page_id 是 INVALID, 说明树空， 返回 End()
        }
        ReadPageGuard guard = bpm_->FetchPageRead(head_guard.As<BPlusTreeHeaderPage>()->root_page_id_);
        //读到 root page, 即 B+ 树的根节点
        head_guard.Drop();
        //提前手动析构 header page
        auto tmp_page = guard.template As<BPlusTreePage>();
        //下面需要一步步寻找参数 key， 先把 guard 的 data 部分解释为 BPlusTreePage.
        //这一步实际上是我们这个 project 的惯例 :
        //拿到 page guard, 然后用 As 成员函数拿到 b_plus_tree_page 的指针。
        while (!tmp_page->IsLeafPage()) {
            //如果不是叶子结点，我就一直找
            auto internal = reinterpret_cast<const InternalPage *>(tmp_page);
            //这里是内部结点， 那就把它 cast 成 InternalPage. InternalPage 是 BPlusTreeInternalPage 的别名。
            //请注意， 只有我们的指针类型正确时候， 我们才能拿到这个类的数据成员和成员函数。
            int slot_num = BinaryFind(internal, key);
            //然后调用辅助函数 BinaryFind 在 page 内部二分查找这个 key， 找到该向下走哪个指针
            if (slot_num == -1) {
                return End();
            }
            //异常处理
            guard = bpm_->FetchPageRead(reinterpret_cast<const InternalPage *>(tmp_page)->ValueAt(slot_num));
            //现在向下走， 根据上方得到的 page id 拿到新的 page guard。
            tmp_page = guard.template As<BPlusTreePage>();
            //然后再用相同方式把 page guard 的数据部分解释为 BPlusTreePage, 继续循环。
        }
        auto *leaf_page = reinterpret_cast<const LeafPage *>(tmp_page);
        //最后跳出循环， 说明找到了叶子结点。
        int slot_num = BinaryFind(leaf_page, key);
        //在叶子节点内部二分查找，找到对应的 key
        if (slot_num != -1) {
            //如果找到了， 构造对应迭代器。这个迭代器可以用于顺序访问所有数据。 本次 project 中不涉及迭代器的处理。
            return INDEXITERATOR_TYPE(bpm_, guard.PageId(), slot_num);
        }
        return End();
    }

    /*
     * Input parameter is void, construct an index iterator representing the end
     * of the key/value pair in the leaf node
     * @return : index iterator
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE {
        return INDEXITERATOR_TYPE(bpm_, -1, -1);
    }

    /**
     * @return Page id of the root of this tree
     */
    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
        ReadPageGuard guard = bpm_->FetchPageRead(header_page_id_);
        auto root_header_page = guard.template As<BPlusTreeHeaderPage>();
        page_id_t root_page_id = root_header_page->root_page_id_;
        return root_page_id;
    }

    /*****************************************************************************
     * UTILITIES AND DEBUG
     *****************************************************************************/

    /**
     * This method is used for test only
     * Read data from file and insert one by one
     */
    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name,
                                        Transaction *txn) {
        int64_t key;
        std::ifstream input(file_name);
        while (input >> key) {
            KeyType index_key;
            index_key.SetFromInteger(key);
            RID rid(key);
            Insert(index_key, rid, txn);
        }
    }

    /**
     * This method is used for test only
     * Read data from file and remove one by one
     */
    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name,
                                        Transaction *txn) {
        int64_t key;
        std::ifstream input(file_name);
        while (input >> key) {
            KeyType index_key;
            index_key.SetFromInteger(key);
            Remove(index_key, txn);
        }
    }

    /**
     * This method is used for test only
     * Read data from file and insert/remove one by one
     */
    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::BatchOpsFromFile(const std::string &file_name,
                                          Transaction *txn) {
        int64_t key;
        char instruction;
        std::ifstream input(file_name);
        while (input) {
            input >> instruction >> key;
            RID rid(key);
            KeyType index_key;
            index_key.SetFromInteger(key);
            switch (instruction) {
                case 'i':
                    Insert(index_key, rid, txn);
                    break;
                case 'd':
                    Remove(index_key, txn);
                    break;
                default:
                    break;
            }
        }
    }

    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
        auto root_page_id = GetRootPageId();
        auto guard = bpm->FetchPageBasic(root_page_id);
        PrintTree(guard.PageId(), guard.template As<BPlusTreePage>());
    }

    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::PrintTree(page_id_t page_id, const BPlusTreePage *page) {
        if (page->IsLeafPage()) {
            auto *leaf = reinterpret_cast<const LeafPage *>(page);
            std::cout << "Leaf Page: " << page_id << "\tNext: " << leaf->GetNextPageId() << std::endl;

            // Print the contents of the leaf page.
            std::cout << "Contents: ";
            for (int i = 0; i < leaf->GetSize(); i++) {
                std::cout << leaf->KeyAt(i);
                if ((i + 1) < leaf->GetSize()) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
            std::cout << std::endl;
        } else {
            auto *internal = reinterpret_cast<const InternalPage *>(page);
            std::cout << "Internal Page: " << page_id << std::endl;

            // Print the contents of the internal page.
            std::cout << "Contents: ";
            for (int i = 0; i < internal->GetSize(); i++) {
                std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i);
                if ((i + 1) < internal->GetSize()) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;
            std::cout << std::endl;
            for (int i = 0; i < internal->GetSize(); i++) {
                auto guard = bpm_->FetchPageBasic(internal->ValueAt(i));
                PrintTree(guard.PageId(), guard.template As<BPlusTreePage>());
            }
        }
    }

    /**
     * This method is used for debug only, You don't need to modify
     */
    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
        if (IsEmpty()) {
            LOG_WARN("Drawing an empty tree");
            return;
        }

        std::ofstream out(outf);
        out << "digraph G {" << std::endl;
        auto root_page_id = GetRootPageId();
        auto guard = bpm->FetchPageBasic(root_page_id);
        ToGraph(guard.PageId(), guard.template As<BPlusTreePage>(), out);
        out << "}" << std::endl;
        out.close();
    }

    /**
     * This method is used for debug only, You don't need to modify
     */
    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::ToGraph(page_id_t page_id, const BPlusTreePage *page,
                                 std::ofstream &out) {
        std::string leaf_prefix("LEAF_");
        std::string internal_prefix("INT_");
        if (page->IsLeafPage()) {
            auto *leaf = reinterpret_cast<const LeafPage *>(page);
            // Print node name
            out << leaf_prefix << page_id;
            // Print node properties
            out << "[shape=plain color=green ";
            // Print data of the node
            out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" "
                    "CELLPADDING=\"4\">\n";
            // Print data
            out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << page_id
                    << "</TD></TR>\n";
            out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
                    << "max_size=" << leaf->GetMaxSize()
                    << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
                    << "</TD></TR>\n";
            out << "<TR>";
            for (int i = 0; i < leaf->GetSize(); i++) {
                out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
            }
            out << "</TR>";
            // Print table end
            out << "</TABLE>>];\n";
            // Print Leaf node link if there is a next page
            if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
                out << leaf_prefix << page_id << "   ->   " << leaf_prefix
                        << leaf->GetNextPageId() << ";\n";
                out << "{rank=same " << leaf_prefix << page_id << " " << leaf_prefix
                        << leaf->GetNextPageId() << "};\n";
            }
        } else {
            auto *inner = reinterpret_cast<const InternalPage *>(page);
            // Print node name
            out << internal_prefix << page_id;
            // Print node properties
            out << "[shape=plain color=pink "; // why not?
            // Print data of the node
            out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" "
                    "CELLPADDING=\"4\">\n";
            // Print data
            out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << page_id
                    << "</TD></TR>\n";
            out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
                    << "max_size=" << inner->GetMaxSize()
                    << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
                    << "</TD></TR>\n";
            out << "<TR>";
            for (int i = 0; i < inner->GetSize(); i++) {
                out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
                // if (i > 0) {
                out << inner->KeyAt(i) << "  " << inner->ValueAt(i);
                // } else {
                // out << inner  ->  ValueAt(0);
                // }
                out << "</TD>\n";
            }
            out << "</TR>";
            // Print table end
            out << "</TABLE>>];\n";
            // Print leaves
            for (int i = 0; i < inner->GetSize(); i++) {
                auto child_guard = bpm_->FetchPageBasic(inner->ValueAt(i));
                auto child_page = child_guard.template As<BPlusTreePage>();
                ToGraph(child_guard.PageId(), child_page, out);
                if (i > 0) {
                    auto sibling_guard = bpm_->FetchPageBasic(inner->ValueAt(i - 1));
                    auto sibling_page = sibling_guard.template As<BPlusTreePage>();
                    if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
                        out << "{rank=same " << internal_prefix << sibling_guard.PageId()
                                << " " << internal_prefix << child_guard.PageId() << "};\n";
                    }
                }
                out << internal_prefix << page_id << ":p" << child_guard.PageId()
                        << "   ->   ";
                if (child_page->IsLeafPage()) {
                    out << leaf_prefix << child_guard.PageId() << ";\n";
                } else {
                    out << internal_prefix << child_guard.PageId() << ";\n";
                }
            }
        }
    }

    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::DrawBPlusTree() -> std::string {
        if (IsEmpty()) {
            return "()";
        }

        PrintableBPlusTree p_root = ToPrintableBPlusTree(GetRootPageId());
        std::ostringstream out_buf;
        p_root.Print(out_buf);

        return out_buf.str();
    }

    INDEX_TEMPLATE_ARGUMENTS
    auto BPLUSTREE_TYPE::ToPrintableBPlusTree(page_id_t root_id)
        -> PrintableBPlusTree {
        auto root_page_guard = bpm_->FetchPageBasic(root_id);
        auto root_page = root_page_guard.template As<BPlusTreePage>();
        PrintableBPlusTree proot;

        if (root_page->IsLeafPage()) {
            auto leaf_page = root_page_guard.template As<LeafPage>();
            proot.keys_ = leaf_page->ToString();
            proot.size_ = proot.keys_.size() + 4; // 4 more spaces for indent

            return proot;
        }

        // draw internal page
        auto internal_page = root_page_guard.template As<InternalPage>();
        proot.keys_ = internal_page->ToString();
        proot.size_ = 0;
        for (int i = 0; i < internal_page->GetSize(); i++) {
            page_id_t child_id = internal_page->ValueAt(i);
            PrintableBPlusTree child_node = ToPrintableBPlusTree(child_id);
            proot.size_ += child_node.size_;
            proot.children_.push_back(child_node);
        }

        return proot;
    }

    template class BPlusTree<GenericKey<4>, RID, GenericComparator<4> >;

    template class BPlusTree<GenericKey<8>, RID, GenericComparator<8> >;

    template class BPlusTree<GenericKey<16>, RID, GenericComparator<16> >;

    template class BPlusTree<GenericKey<32>, RID, GenericComparator<32> >;

    template class BPlusTree<GenericKey<64>, RID, GenericComparator<64> >;
} // namespace bustub
