# Project5 : B+ tree in Database

> 本文档由readme改编而来，为个人写B+树提供帮助


# 基础知识

在开始这个 project 之前， 我们需要了解一些基础知识。 由于课上已学习过 B 树与 B+ 树，这里没有对 B 树与 B+ 树进行介绍， 如有需要请查阅相关课程 PPT。如果你对 B+ 树进行操作后的结构有疑惑， 请在 https://www.cs.usfca.edu/~galles/visualization/BPlusTree.html 网站上进行尝试。 此外， 这个博客的动图非常生动： https://zhuanlan.zhihu.com/p/149287061。

## 存储引擎与 B+ 树

<img src="https://notes.sjtu.edu.cn/uploads/upload_bfde29d73741b26103fce71094eae7e4.png" width="800">

## 存储引擎与数据页(optional)

根据上方的表格图像，我们每行都存储了一条数据信息。但如果用户执行查询操作， 我们并不能以 "行" 为单位读取数据， 否则一次磁盘 IO 只能处理一行， 执行效率过低。 我们这里采取的策略是以 `page`（页）为单位进行磁盘 IO。 同时， 我们的索引结点也以 `page` 为基本单位进行存储， 即每个索引结点（中间结点）都对应一个 `page`。 我们可以简单地将一个 `page` 视为是固定大小的一块存储空间。 通过打包一系列数据进入同一个 page， 可以实现减少磁盘 IO 的效果。这里我们并不需要了解 `page` 的细节，与磁盘的 IO 操作已被封装为以下几个函数: `FetchPageRead`，`FetchPageWrite`。我会在之后详细说明这两个函数。 

<img src="https://notes.sjtu.edu.cn/uploads/upload_5879015cd51b787ca781b64ac3e5e7b2.png" width="800">

# 主体任务

请你修改 `src/include/storage/index/b_plus_tree.h` 和 `src/storage/index/b_plus_tree.cpp`, 实现 b+ 树的查找、插入和删除函数。 在此之后， 请你完善 b+ 树的查找、插入、删除函数， 使其线程安全。

# 熟悉项目代码

## page

 `src/include/storage/page/page.h `:

```c++
  char* data_;//数据
  page_id_t page_id_ = INVALID_PAGE_ID;//Page的唯一标识
  int pin_count_ = 0;
  bool is_dirty_ = false;
  ReaderWriterLatch rwlatch_;//读写锁
```


## b_plus_tree_page（包在page->data中）

 `src/include/storage/page/b_plus_tree_page.h`:

```cpp
//b+树page的基类
class BPlusTreePage{
  auto IsLeafPage() const -> bool;
  void SetPageType(IndexPageType page_type);

  auto GetSize() const -> int;
  void SetSize(int size);
  void IncreaseSize(int amount);

  auto GetMaxSize() const -> int;
  void SetMaxSize(int max_size);
  auto GetMinSize() const -> int;

  IndexPageType page_type_;
  int size_;
  int max_size_;
};
```

 `src/include/storage/page/b_plus_tree_header_page.h`

```cpp
//b+树page的根节点类
class BPlusTreeHeaderPage{
  page_id_t root_page_id_;
};
```

 `src/include/storage/page/b_plus_tree_internal_page.h`, 

`src/include/storage/page/b_plus_tree_leaf_page.h`:

```cpp
INDEX_TEMPLATE_ARGUMENTS
// 这是为了写模板简便定义的一个宏：
// #define INDEX_TEMPLATE_ARGUMENTS template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTreeInternalPage : public BPlusTreePage{
  void Init(int max_size = INTERNAL_PAGE_SIZE);
  auto KeyAt(int index) const -> KeyType;
  auto ValueAt(int index) const -> ValueType;
  void SetKeyAt(int index, const KeyType& key);
  auto ValueIndex(const ValueType& value) const -> int;
  private:
  // Flexible array member for page data.
  MappingType array_[0];
  //MappingType 是这样定义的一个宏： #define MappingType std::pair<KeyType, ValueType>
};
```

```cpp
INDEX_TEMPLATE_ARGUMENTS
// 这是为了写模板简便定义的一个宏：
// #define INDEX_TEMPLATE_ARGUMENTS template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTreeLeafPage : public BPlusTreePage{
  void Init(int max_size = LEAF_PAGE_SIZE);
  auto GetNextPageId() const -> page_id_t;
  void SetNextPageId(page_id_t next_page_id);
  auto KeyAt(int index) const -> KeyType;
  auto ValueAt(int index) const -> ValueType;
  void SetKeyAt(int index, const KeyType& key);
  void SetValueAt(int index, const ValueType & value);
  private:
  page_id_t next_page_id_;
  // Flexible array member for page data.
  MappingType array_[0];
  //MappingType 是这样定义的一个宏： #define MappingType std::pair<KeyType, ValueType>
};
```

`Internal page` 对应 B+ 树的内部结点， `leaf page` 对应 B+ 树的叶子结点。对于 `internal page`, 它存储着 n 个 索引 key 和 n + 1 个指向 `children page` 的指针。(由于我们以数组形式存储， 因此第一个数组元素对应的索引项无实际意义)  

对于 `LeafPage`, 它存储着 n 个 索引 key 和 n 个对应的数据行 ID。 这里的 `"KeyAt", "SetKeyAt", "ValueAt", "SetValueAt"` 可用于键值对的查询与更新， 会在 B+ 树的编写中用到。

所有的叶子节点形成一个链表， 辅助函数 `GetNextPageId` 和 `SetNextPageId` 可用于维护这个链表。

`Init` 函数可用于手动刷新这个 `b_plus_tree_page`， 通常你不会手动调用这个成员函数， 但如果你的实现需要用到刷新 `b_plus_tree_page`, 你可以考虑调用它。

别忘了可以使用 `BPlusTreePage` 的成员函数 (如 `GetSize`, `IncreaseSize`)！

## page_guard

 `src/include/storage/page/page_guard.h`。 我们使用 RAII 思想为 `page` 写了一个封装后的新类: `page_guard`。 

下面是 `page_guard` 的原型：

```cpp
// PageGuard 系列逻辑上的基类 BasicPageGuard
// 是创建新页时的返回类型
// 它不会自动上锁 + 解锁！
class BasicPageGuard{
  void Drop();
  // Drop 函数相当于手动析构
  // 请注意， 析构时会自动写入磁盘
  auto UpgradeRead() -> ReadPageGuard;
  auto UpgradeWrite() -> WritePageGuard;
  auto PageId() -> page_id_t { return page_->GetPageId(); }
  auto GetData() -> const char* { return page_->GetData(); }
  template <class T>
  auto As() -> const T*{
    return reinterpret_cast<const T*>(GetData());
  }
  auto GetDataMut() -> char*{
    is_dirty_ = true;
    return page_->GetData();
  }
  template <class T>
  auto AsMut() -> T*{
    return reinterpret_cast<T*>(GetDataMut());
  }
  private:
  friend class ReadPageGuard;
  friend class WritePageGuard;

  BufferPoolManager* bpm_{nullptr};
  Page* page_{nullptr};
  bool is_dirty_{false};
};

// ReadPageGuard 会在构造时获取读锁， 析构时释放读锁
class ReadPageGuard{
  void Drop();
  auto PageId() -> page_id_t { return guard_.PageId(); }
  auto GetData() -> const char* { return guard_.GetData(); }
  template <class T>
  auto As() -> const T*{
    return guard_.As<T>();
  }
  private:
    BasicPageGuard guard_;
    bool unlock_guard{false};
};

// WritePageGuard 会在构造时获取写锁， 析构时释放写锁
class WritePageGuard{
  void Drop();
  auto PageId() -> page_id_t { return guard_.PageId(); }
  auto GetData() -> const char* { return guard_.GetData(); }
  template <class T>
  auto As() -> const T*{
    return guard_.As<T>();
  }
  auto GetDataMut() -> char* { return guard_.GetDataMut(); }
  template <class T>
  auto AsMut() -> T*{
    return guard_.AsMut<T>();
  }
  private:
    BasicPageGuard guard_;
    bool unlock_guard{false};
};
```

在这个 .h 文件里你还需要关注 `page_guard` 的 `As` 函数

```cpp
auto As() -> const T*{
  return reinterpret_cast<const T*>(GetData());
}
```

另外， `AsMut` 函数即为 `As` 函数的 非 const 版本， 返回值为 `T * ` 而非 `const T * `:

```cpp
auto AsMut() -> T*{
  return reinterpret_cast<T*>(GetDataMut());
}
```

它们可以获取 `page_guard` 封装起来的 `page` 的 `data` 区域, 将这块区域重新解释为某一类型。

此外， `Drop` 成员函数相当于手动调用析构函数： 它会释放对于 `page` 的所有权， 释放该 `page` 的锁， 并将内容写入磁盘。

另外， `UpgradeRead` 与 `UpgradeWrite` 可以将 `BasicPageGuard` 升级为 `ReadPageGuard` / `WritePageGuard`, 相当于获取并自动管理读锁 / 写锁。 如果你希望新建一个 `page` 之后立刻为它上锁， 这两个函数可能会派生用场。

```cpp
auto UpgradeRead() -> ReadPageGuard;
auto UpgradeWrite() -> WritePageGuard;
```

`PageId` 成员函数可以返回该 `page_guard` 对应的 `page_id`。 

```cpp
auto PageId() -> page_id_t { return guard_.PageId(); }
```

### page_guard 使用示例

请看我在 `src/storage/index/b_plus_tree.cpp` 给出的示例函数 `IsEmpty`:

```cpp
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const  ->  bool
{
  ReadPageGuard guard = bpm_ -> FetchPageRead(header_page_id_);
  auto root_header_page = guard.template As<BPlusTreeHeaderPage>();
  bool is_empty = root_header_page -> root_page_id_ == INVALID_PAGE_ID;
  // Just check if the root_page_id is INVALID
  // usage to fetch a page:
  // fetch the page guard   ->   call the "As" function of the page guard
  // to reinterprete the data of the page as "BPlusTreePage"
  return is_empty;
}
```

这里的 `guard.template As<BPlusTreeHeaderPage>();` 即为获取我们读到的 `ReadPageGuard` 封装的 `page` 的 `data` 区域， 将这块区域重新解释为 `BPlusTreeHeaderPage` 类型。 也就是， 我们获取了一个用来读的 `page`， 然后把这个 `page` 里面的数据解释为 `BPlusTreeHeaderPage`， 从而读取 `header page` 里的 `root_page_id` 信息， 检查是否是 `INVALID`.

上方有一个很奇怪的函数， 叫做 `bpm_ -> FetchPageRead`, 这是什么呢？ 请接着看。

## FetchPage by buffer pool manager

请见 `src/include/buffer/buffer_pool_manager.h `. 这份代码实际上是用于实现缓存池， 具体内容你不需要理解。

我们需要用到的函数有：

```cpp
  auto FetchPageRead(page_id_t page_id) -> ReadPageGuard;
  auto FetchPageWrite(page_id_t page_id) -> WritePageGuard;
  auto NewPageGuarded(page_id_t* page_id, AccessType access_type = AccessType::Unknown) -> BasicPageGuard;
```

请你在本次 project 中将它们视为一个黑盒。

### Create New Page

`NewPageGuarded` 函数用于新建一个新的 `page`， 为其分配它的唯一标识 `PAGE ID` 并将该 `PAGE ID` 填入参数 `page_id` 中， 最后以 `page_guard` 类的形式返回它。


下面是一个使用 `NewPageGuarded` 函数的例子：

```cpp
page_id_t root_page_id;
// 这个临时变量用于保存此时分配的 page id

auto new_page_guard = bpm_ -> NewPageGuarded(&root_page_id);
// 调用 `NewPageGuarded`, 该函数会新建一个 Page， 并为它分配 ID
// 然后填入我们传入的 root_page_id 中
// 最后以 page_guard 的形式返回给我们
// 此时返回的是 page_guard 的基类 BasicPageGuard 
// 请注意， 此时该 page 未上锁。
// 如果你希望为它上锁， 可以使用上面提到的 UpgradeRead / UpgradeWrite
// 你也可以尝试使用其他方法为 BasicPageGuard 上锁
```

### Fetch Page

`bpm_ -> FetchPageRead` 和 `bpm_ -> FetchPageWrite` 用于通过磁盘 IO ， 根据 `page id` 得到一个用于读的 `ReadPageGuard` 或者一个用于写 `WritePageGuard`. 如果你好奇其中的细节(如缓存池)， 请私信我。 `ReadPageGuard` 会自动获取该 `page` 的读锁， 并在析构时释放读锁。 `WritePageGuard` 会自动获取该 `page` 的写锁， 并在析构时释放写锁。


下面是一些使用 `FetchPageRead / FetchPageWrite` 的例子：

```cpp
/*
 * Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType& key)  ->  INDEXITERATOR_TYPE
{
  ReadPageGuard head_guard = bpm_ -> FetchPageRead(header_page_id_);
    //读到 header page

  if (head_guard.template As<BPlusTreeHeaderPage>() -> root_page_id_ == INVALID_PAGE_ID)
  {
    return End();
  }
    //如果 header page 存的 root_page_id 是 INVALID, 说明树空， 返回 End()

  ReadPageGuard guard = bpm_ -> FetchPageRead(head_guard.As<BPlusTreeHeaderPage>() -> root_page_id_);
    //读到 root page, 即 B+ 树的根节点

  head_guard.Drop();
  //提前手动析构 header page

  auto tmp_page = guard.template As<BPlusTreePage>();
    //下面需要一步步寻找参数 key， 先把 guard 的 data 部分解释为 BPlusTreePage. 这一步实际上是我们这个 project 的惯例 : 拿到 page guard, 然后用 As 成员函数拿到 b_plus_tree_page 的指针。

  while (!tmp_page -> IsLeafPage())
  { 
    //如果不是叶子结点，我就一直找

    auto internal = reinterpret_cast<const InternalPage*>(tmp_page);
    //这里是内部结点， 那就把它 cast 成 InternalPage. InternalPage 是 BPlusTreeInternalPage 的别名。请注意， 只有我们的指针类型正确时候， 我们才能拿到这个类的数据成员和成员函数。

    int slot_num = BinaryFind(internal, key);
    //然后调用辅助函数 BinaryFind 在 page 内部二分查找这个 key， 找到该向下走哪个指针

    if (slot_num == -1)
    {
      return End();
    }
    //异常处理

    guard = bpm_ -> FetchPageRead(reinterpret_cast<const InternalPage*>(tmp_page) -> ValueAt(slot_num));
    //现在向下走， 根据上方得到的 page id 拿到新的 page guard。

    tmp_page = guard.template As<BPlusTreePage>();
    //然后再用相同方式把 page guard 的数据部分解释为 BPlusTreePage, 继续循环。
  }
  auto* leaf_page = reinterpret_cast<const LeafPage*>(tmp_page);
  //最后跳出循环， 说明找到了叶子结点。

  int slot_num = BinaryFind(leaf_page, key);
    //在叶子节点内部二分查找，找到对应的 key

  if (slot_num != -1)
  {
    //如果找到了， 构造对应迭代器。这个迭代器可以用于顺序访问所有数据。 本次 project 中不涉及迭代器的处理。
    return INDEXITERATOR_TYPE(bpm_, guard.PageId(), slot_num);
  } 
  return End();
}
```

### Write Page

你可能会问， 为什么没有一个函数把 `page` 写回磁盘呢？ 实际上， 只要你析构 `PageGuard`, 对应的 `page` 就会自动写回磁盘。

(这里有更多关于缓存池的细节， 如果感兴趣可以了解一下数据库缓存池)

## B+ 树核心代码

请见 `src/include/storage/index/b_plus_tree.h` 与 `src/storage/index/b_plus_tree.cpp`

请注意，你并不需要关心任何和 `BufferPoolManager`, `Transaction` 相关的数据成员和函数参数。 

你需要实现 B+ 树的查找 (`GetValue`)、插入（`Insert`）与删除 (`Remove`) 操作。 请注意， 整个 B+ 树存储于磁盘上， 因此每个结点都是以 `page` 形式存在， 需要使用 `FetchPageRead / FetchPageWrite` 函数将 `page` 从磁盘拿到内存中。


```cpp
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType& key,
                              std::vector<ValueType>* result, Transaction* txn)
     ->  bool
{
  //Your code here
  return true;
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
auto BPLUSTREE_TYPE::Insert(const KeyType& key, const ValueType& value,
                            Transaction* txn)  ->  bool
{
  //Your code here
  return true;
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
void BPLUSTREE_TYPE::Remove(const KeyType& key, Transaction* txn)
{
  //Your code here
}
```

此外， 我们额外留心一下 B+ 树的构造函数：

```cpp
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id,
                          BufferPoolManager* buffer_pool_manager,
                          const KeyComparator& comparator, int leaf_max_size,
                          int internal_max_size)
    : index_name_(std::move(name)),
      bpm_(buffer_pool_manager),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id)
{
  WritePageGuard guard = bpm_ -> FetchPageWrite(header_page_id_);
  // In the original bpt, I fetch the header page
  // thus there's at least one page now
  auto root_header_page = guard.template AsMut<BPlusTreeHeaderPage>();
  // reinterprete the data of the page into "HeaderPage"
  root_header_page -> root_page_id_ = INVALID_PAGE_ID;
  // set the root_id to INVALID
}
```

这里需要注意的是， 我们传入了一个比较函数的函数对象， 如果你希望对 `key` 进行比较， 请使用这里的 `comparator_` 函数对象, 它的实现如下:

```cpp
  inline auto operator()(const GenericKey<KeySize>& lhs,
                         const GenericKey<KeySize>& rhs) const -> int
  {
    uint32_t column_count = key_schema_->GetColumnCount();

    for (uint32_t i = 0; i < column_count; i++)
    {
      Value lhs_value = (lhs.ToValue(key_schema_, i));
      Value rhs_value = (rhs.ToValue(key_schema_, i));

      if (lhs_value.CompareLessThan(rhs_value) == CmpBool::CmpTrue)
      {
        return -1;
      }
      if (lhs_value.CompareGreaterThan(rhs_value) == CmpBool::CmpTrue)
      {
        return 1;
      }
    }
    // equals
    return 0;
  }
```

如果你希望在测试时修改索引的值为某一整数， 你可以使用 `SetFromInteger`, 如

```cpp
  KeyType index_key;
  index_key.SetFromInteger(key);
  Remove(index_key, txn);
```











## Context

请见 `src/include/storage/index/b_plus_tree.h`.

`Context` 类可用于编写 B+ 树的螃蟹法则。 你可以使用它存储一条链上的锁， 也可以自己实现一个数据结构实现螃蟹法则。

```cpp
/**
 * @brief Definition of the Context class.
 *
 * Hint: This class is designed to help you keep track of the pages
 * that you're modifying or accessing.
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
};
```

### B+ 树加锁方法（Optinal)

B+ 树加锁采用 "螃蟹法则"。具体请见下图。 

<img src="https://notes.sjtu.edu.cn/uploads/upload_9c4c517643c2ab7eba276408e2233d8f.png" width="800">

<img src="https://notes.sjtu.edu.cn/uploads/upload_38c70fad05997aaf4ffb671539067d16.png" width="800">


这里我用大白话再解释一遍： 拿 `insert` 函数举例， 我们在搜索路径上每次都先拿 parent 结点的锁， 然后拿 child 结点的锁， 如果 child 结点是 "安全" 的， 就自上而下释放一路走下来所有 parent 结点的锁。（自上而下和自下而上都可以，但是自上而下能更快冲淡堵塞）

所谓的 "安全" 如何定义？ 只要 `child page` 插入时不满， 或者删除时至少半满，那就安全。 

进阶螃蟹法则（乐观锁）的意思就是， 对于写的线程仍然先拿读锁， 如果发现遇到了不安全的 `leaf page`, 可能引起上方的 `internal page` 也发生分裂， 那我立刻放弃继续执行乐观锁策略， 然后重新开始，依照普通的螃蟹法则进行写入的操作。
