#include "storage/index/b_plus_tree.h"

#include <sstream>
#include <string>
#include <vector>

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
    auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *txn) -> bool {
        ReadPageGuard head_guard = bpm_->FetchPageRead(header_page_id_);
        //读到 header page
        if (head_guard.template As<BPlusTreeHeaderPage>()->root_page_id_ == INVALID_PAGE_ID) {
            return false;
        }
        //如果 header page 存的 root_page_id 是 INVALID, 说明树空， 返回 false
        ReadPageGuard guard = bpm_->FetchPageRead(head_guard.As<BPlusTreeHeaderPage>()->root_page_id_);
        head_guard.Drop();
        auto tmp_page = guard.template As<BPlusTreePage>();
        //下面需要一步步寻找参数 key， 先把 guard 的 data 部分解释为 BPlusTreePage.
        while (!tmp_page->IsLeafPage()) {
            //如果不是叶子结点，我就一直找
            auto internal = reinterpret_cast<const InternalPage *>(tmp_page);
            //这里是内部结点， 那就把它 cast 成 InternalPage. InternalPage 是 BPlusTreeInternalPage 的别名。
            int slot_num = BinaryFind(internal, key);
            if (slot_num == -1) {
                return false;
            }
            guard = bpm_->FetchPageRead(reinterpret_cast<const InternalPage *>(tmp_page)->ValueAt(slot_num));
            //现在向下走， 根据上方得到的 page id 拿到新的 page guard。
            tmp_page = guard.template As<BPlusTreePage>();
            //然后再用相同方式把 page guard 的数据部分解释为 BPlusTreePage, 继续循环。
        }
        auto *leaf_page = reinterpret_cast<const LeafPage *>(tmp_page);
        //最后跳出循环， 说明找到了叶子结点。
        int slot_num = BinaryFind(leaf_page, key);
        //在叶子节点内部二分查找，找到对应的 key
        if (slot_num != -1 && comparator_(leaf_page->KeyAt(slot_num), key) == 0) {
            result->push_back(leaf_page->ValueAt(slot_num));
            return true;
        }
        return false;
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
    auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *txn) -> bool {
        //记录路径，方便后续向上分裂（干脆拿context来用了，虽然锁写的不一定对，但是可以当做路径用还是没啥问题的）
        Context path;
        // WritePageGuard head_guard
        path.header_page_ = bpm_->FetchPageWrite(header_page_id_);
        auto head = path.header_page_->template AsMut<BPlusTreeHeaderPage>();
        //树为空的情形已经完成，不要动了！！！
        if (head->root_page_id_ == INVALID_PAGE_ID) {
            //树为空，新建一棵树，插入key和value
            auto root_guard = bpm_->NewPageGuarded(&head->root_page_id_);
            auto leaf_page = root_guard.AsMut<LeafPage>();
            leaf_page->Init(leaf_max_size_);
            leaf_page->SetSize(1);
            leaf_page->SetAt(0, key, value);
            path.header_page_ = std::nullopt; // unlock header_page
            return true;
        }
        //树非空
        path.root_page_id_ = head->root_page_id_;
        //获取根的写锁
        path.write_set_.push_back(bpm_->FetchPageWrite(path.root_page_id_));
        // auto tmp_page = path.write_set_.back().template As<BPlusTreePage>();
        //如果孩子是安全的，那就可以把head的锁释放掉了
        if (Safe_Insert(path.write_set_.back().As<BPlusTreePage>())) {
            path.header_page_ = std::nullopt; // unlock header_page
        }
        //开始找叶子
        auto page = path.write_set_.back().As<BPlusTreePage>();
        while (!page->IsLeafPage()) {
            //获取现在的这一层，然后向下找next层（用二分）
            auto now = path.write_set_.back().As<InternalPage>();
            auto slot_num = BinaryFind(now, key);
            // if (slot_num == -1) {
            //     std::cerr << "wrong! cannot find in internal page" << std::endl;
            //     return false;
            //     //不该出现
            // }
            auto next = now->ValueAt(slot_num);
            path.write_set_.push_back(bpm_->FetchPageWrite(next));
            //如果孩子安全，上面的锁都可以不用了，因为不会到这么上面来
            auto child = path.write_set_.back().template As<BPlusTreePage>();
            if (Safe_Insert(child)) {
                while (path.write_set_.size() > 1) {
                    path.write_set_.pop_front();
                }
            }
            page = child;
        }
        //找到leaf，注意这个要用AsMut了，要修改
        auto leaf_guard = path.write_set_.back().template AsMut<LeafPage>();
        //重新解释为叶子结点
        auto *leaf_page = reinterpret_cast<LeafPage *>(leaf_guard);
        int slot_num = BinaryFind(leaf_page, key);
        if (slot_num != -1 && comparator_(leaf_page->KeyAt(slot_num), key) == 0) {
            path.clear();
            return false;
        }
        slot_num++;
        leaf_page->IncreaseSize(1);
        //全部后移一位
        for (int i = leaf_page->GetSize(); i > slot_num; --i) {
            leaf_page->SetAt(i, leaf_page->KeyAt(i - 1), leaf_page->ValueAt(i - 1));
        }
        //插入
        leaf_page->SetAt(slot_num, key, value);
        //如果没有超限，那么插入结束（这部分应该完成了）
        if (leaf_page->GetSize() < leaf_page->GetMaxSize()) {
            return true;
        }
        //上面是最基本的插入，没有附加任何向上的操作，下面开始考虑这种情况。Not done yet----By smiling 4.29

        //该叶子结点满了，需要分裂
        auto new_page = 0;
        auto new_guard = bpm_->NewPageGuarded(&new_page);
        auto new_leaf = new_guard.AsMut<LeafPage>();
        //将要分裂的page和新增的page相连,以及初始化
        //leaf ---> leaf_next  变成
        //leaf ---> new_leaf ---> leaf_next
        new_leaf->Init(leaf_max_size_);
        new_leaf->SetSize(leaf_page->GetSize() - leaf_page->GetMinSize());
        new_leaf->SetNextPageId(leaf_page->GetNextPageId());
        leaf_page->SetNextPageId(new_guard.PageId());
        //将后半部分的key和value都放到新的page上
        for (int i = leaf_page->GetMinSize(); i < leaf_page->GetSize(); ++i) {
            new_leaf->SetAt(i - leaf_page->GetMinSize(), leaf_page->KeyAt(i), leaf_page->ValueAt(i));
        }
        leaf_page->SetSize(leaf_page->GetMinSize());
        //要上去的那个key，向上插入，调用辅助函数InsertUp来插入
        KeyType up_key = new_leaf->KeyAt(0);
        //需要处理几个？现在我们在leaf，所以需要处理的是path中除了leaf的所有internal，总计-1个
        Insert_Up(up_key, new_guard.PageId(), path);
        return true;
    }

    INDEX_TEMPLATE_ARGUMENTS
    bool BPLUSTREE_TYPE::Safe_Insert(const BPlusTreePage *page) {
        if (page->IsLeafPage()) {
            return page->GetSize() + 1 < page->GetMaxSize();
        } else {
            return page->GetSize() < page->GetMaxSize();
        }
    }

    INDEX_TEMPLATE_ARGUMENTS
    void BPLUSTREE_TYPE::Insert_Up(const KeyType &key, page_id_t right_child, Context &path) {
        //到head了  done
        if (path.write_set_.size() == 1) {
            //到head了。说明根满了，需要分裂-->需要换一个根来用。树的深度会+1
            int new_root_id;
            auto new_root_guard = bpm_->NewPageGuarded(&new_root_id);
            auto new_root = new_root_guard.AsMut<InternalPage>();

            new_root->Init(internal_max_size_);
            new_root->SetSize(2);
            // new_root->SetKeyAt(0, key);
            //根节点只有一个，两个page箭头指向原来的根以及根在上一次分裂出来的page
            new_root->SetKeyAt(1, key);
            new_root->SetValueAt(0, path.write_set_[0].PageId());
            new_root->SetValueAt(1, right_child);
            //修改header！用AsMut
            auto header_page = path.header_page_->AsMut<BPlusTreeHeaderPage>();
            header_page->root_page_id_ = new_root_id;

            return;
        }

        auto father_guard = path.write_set_[path.write_set_.size() - 2].AsMut<InternalPage>();
        //如果父亲不用分裂，那这就是最后一次了
        if (father_guard->GetSize() < father_guard->GetMaxSize()) {
            //我要插的是key的后面，所以记得+1
            int slot_num = BinaryFind(father_guard, key);
            father_guard->IncreaseSize(1);
            //同样的，后面的后挪一位
            for (int i = father_guard->GetSize() - 1; i > slot_num; --i) {
                father_guard->SetKeyAt(i + 1, father_guard->KeyAt(i));
                father_guard->SetValueAt(i + 1, father_guard->ValueAt(i));
            }
            // father_guard->SetKeyAt(slot_num, key);
            // father_guard->SetValueAt(slot_num, right_child);
            father_guard->SetKeyAt(slot_num + 1, key);
            father_guard->SetValueAt(slot_num + 1, right_child);
            return;
        }
        //如果还得分裂，那就要看插到那边，然后递归向上走
        auto new_father_id = 0;
        auto new_father_guard = bpm_->NewPageGuarded(&new_father_id);
        auto new_father_page = new_father_guard.AsMut<InternalPage>();

        int slot_num = BinaryFind(father_guard, key) + 1;
        int change = father_guard->GetMinSize();
        int new_size = father_guard->GetMaxSize() + 1 - change;
        //分类讨论
        if (slot_num < change) {
            //插到左边，先设置大小
            new_father_page->Init(internal_max_size_);
            new_father_page->SetSize(new_size);
            //挪动
            for (int i = change; i < father_guard->GetSize(); ++i) {
                new_father_page->SetKeyAt(i - change + 1, father_guard->KeyAt(i));
                new_father_page->SetValueAt(i - change + 1, father_guard->ValueAt(i));
            }
            new_father_page->SetKeyAt(0, father_guard->KeyAt(change - 1));
            new_father_page->SetValueAt(0, father_guard->ValueAt(change - 1));
            for (int i = change; i >= slot_num; --i) {
                father_guard->SetKeyAt(i + 1, father_guard->KeyAt(i));
                father_guard->SetValueAt(i + 1, father_guard->ValueAt(i));
            }
            father_guard->SetKeyAt(slot_num, key);
            father_guard->SetValueAt(slot_num, right_child);
        }
        else if (slot_num == change) {
            //中间
            new_father_page->Init(internal_max_size_);
            new_father_page->SetSize(new_size);
            for (int i = change; i < father_guard->GetSize(); ++i) {
                new_father_page->SetKeyAt(i - change + 1, father_guard->KeyAt(i));
                new_father_page->SetValueAt(i - change + 1, father_guard->ValueAt(i));
            }
            new_father_page->SetKeyAt(0, key);
            new_father_page->SetValueAt(0, right_child);
        }
        else {
            //右边
            new_father_page->Init(internal_max_size_);
            new_father_page->SetSize(new_size);
            for (int i = change; i < father_guard->GetSize(); ++i) {
                new_father_page->SetKeyAt(i - change, father_guard->KeyAt(i));
                new_father_page->SetValueAt(i - change, father_guard->ValueAt(i));
            }
            slot_num -= change;
            for (int i = new_father_page->GetSize(); i >= slot_num; --i) {
                new_father_page->SetKeyAt(i + 1, new_father_page->KeyAt(i));
                new_father_page->SetValueAt(i + 1, new_father_page->ValueAt(i));
            }
            new_father_page->SetKeyAt(slot_num, key);
            new_father_page->SetValueAt(slot_num, right_child);
        }

        //修改大小为minsize
        father_guard->SetSize(change);
        //向上递归
        path.write_set_.pop_back();
        Insert_Up(new_father_page->KeyAt(0), new_father_id, path);
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
    auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
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
