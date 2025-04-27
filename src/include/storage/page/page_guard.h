#pragma once

#include "storage/page/page.h"

namespace bustub {
    class BufferPoolManager;
    class ReadPageGuard;
    class WritePageGuard;

    // PageGuard 系列逻辑上的基类 BasicPageGuard
    // 是创建新页时的返回类型
    // 它不会自动上锁 + 解锁！
    /**
     * 我们在 page.h 中可以看到， 每个 page 都附带了一个读写锁。
     * 为了避免遗忘释放锁这一操作， 我们使用 RAII 思想为 page 写了一个封装后的新类: page_guard。
     *
     * 简而言之， page_guard 会在构造函数里获取锁， 在析构函数里释放锁。
     * 我们对应设计了 BasicPageGuard 和它的逻辑上的派生类 ReadPageGuard 、 WritePageGuard.
     *
     * 在这个 .h 文件里你还需要关注 page_guard 的 As 函数
     * 另外， AsMut 函数即为 As 函数的 非 const 版本， 返回值为 T *  而非 const T *
     *
     * 它们可以获取 page_guard 封装起来的 page 的 data 区域, 将这块区域重新解释为某一类型。
     *
     * 此外， Drop 成员函数相当于手动调用析构函数： 它会释放对于 page 的所有权， 释放该 page 的锁， 并将内容写入磁盘。
     * 另外， UpgradeRead 与 UpgradeWrite 可以将 BasicPageGuard 升级为 ReadPageGuard / WritePageGuard,
     * 相当于获取并自动管理读锁 / 写锁。 如果你希望新建一个 page 之后立刻为它上锁， 这两个函数可能会派生用场。
     *
     * PageId 成员函数可以返回该 page_guard 对应的 page_id。
     *
     * 其他未提及的成员函数不是本次 project 必要的成员函数， 你可以忽略。
     *
     * 使用示例:请看我在 src/storage/index/b_plus_tree.cpp 给出的示例函数 IsEmpty:
     */
    class BasicPageGuard {
    public:
        BasicPageGuard() = default;

        BasicPageGuard(BufferPoolManager *bpm, Page *page) : bpm_(bpm), page_(page) {
        }

        BasicPageGuard(const BasicPageGuard &) = delete;

        auto operator=(const BasicPageGuard &) -> BasicPageGuard & = delete;

        // 请注意， 我们删掉了拷贝构造和拷贝构造函数哦
        BasicPageGuard(BasicPageGuard &&that) noexcept;

        void Drop();

        // Drop 函数相当于手动析构
        // 请注意， 析构时会自动写入磁盘
        auto operator=(BasicPageGuard &&that) noexcept -> BasicPageGuard &;

        // 如果有需要， 请使用移动构造函数和移动赋值函数
        // (对于内部实现而言， 这样才能保证 PageGuard 对于 Pin_Count 和锁的正确管理)
        ~BasicPageGuard();

        auto UpgradeRead() -> ReadPageGuard;

        auto UpgradeWrite() -> WritePageGuard;

        auto PageId() -> page_id_t { return page_->GetPageId(); }

        auto GetData() -> const char * { return page_->GetData(); }

        template<class T>
        auto As() -> const T * {
            return reinterpret_cast<const T *>(GetData());
        }

        auto GetDataMut() -> char * {
            is_dirty_ = true;
            return page_->GetData();
        }

        template<class T>
        auto AsMut() -> T * {
            return reinterpret_cast<T *>(GetDataMut());
        }

    private:
        friend class ReadPageGuard;
        friend class WritePageGuard;

        BufferPoolManager *bpm_{nullptr};
        Page *page_{nullptr};
        bool is_dirty_{false};
    };

    // ReadPageGuard 会在构造时获取读锁， 析构时释放读锁
    class ReadPageGuard {
    public:
        ReadPageGuard() = default;

        ReadPageGuard(BufferPoolManager *bpm, Page *page);

        ReadPageGuard(const ReadPageGuard &) = delete;

        auto operator=(const ReadPageGuard &) -> ReadPageGuard & = delete;

        ReadPageGuard(ReadPageGuard &&that) noexcept;

        auto operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard &;

        void Drop();

        ~ReadPageGuard();

        auto PageId() -> page_id_t { return guard_.PageId(); }

        auto GetData() -> const char * { return guard_.GetData(); }

        template<class T>
        auto As() -> const T * {
            return guard_.As<T>();
        }

    private:
        BasicPageGuard guard_;
        // 我们存了一个 BasicPageGuard
        // 实现了逻辑上的继承
        bool unlock_guard{false};
    };

    // WritePageGuard 会在构造时获取写锁， 析构时释放写锁
    class WritePageGuard {
    public:
        WritePageGuard() = default;

        WritePageGuard(BufferPoolManager *bpm, Page *page);

        WritePageGuard(const WritePageGuard &) = delete;

        auto operator=(const WritePageGuard &) -> WritePageGuard & = delete;

        WritePageGuard(WritePageGuard &&that) noexcept;

        auto operator=(WritePageGuard &&that) noexcept -> WritePageGuard &;

        void Drop();

        ~WritePageGuard();

        auto PageId() -> page_id_t { return guard_.PageId(); }

        auto GetData() -> const char * { return guard_.GetData(); }

        template<class T>
        auto As() -> const T * {
            return guard_.As<T>();
        }

        auto GetDataMut() -> char * { return guard_.GetDataMut(); }

        template<class T>
        auto AsMut() -> T * {
            return guard_.AsMut<T>();
        }

    private:
        BasicPageGuard guard_;
        // 我们存了一个 BasicPageGuard
        // 实现了逻辑上的继承
        bool unlock_guard{false};
    };
} // namespace bustub
