//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// page.h
//
// Identification: src/include/storage/page/page.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <cstring>
#include <iostream>

#include "common/config.h"
#include "common/rwlatch.h"

namespace bustub {
    /**
     * Page is the basic unit of storage within the database system. Page provides a
     * wrapper for actual data pages being held in main memory. Page also contains
     * book-keeping information that is used by the buffer pool manager, e.g. pin
     * count, dirty flag, page id, etc.
     * 所以， 我们为每个 page 都实现了一个读写锁 rwlatch_, 我们之后的加锁是为 page 加锁。
     * 其次， 我们的 page 包含一个 char * 区域存储 page 内部包含的数据。 page_id 是该 page 的唯一标识。其他成员你可以忽略。
     */
    class Page {
        // There is book-keeping information inside the page that should only be
        // relevant to the buffer pool manager.
        friend class BufferPoolManager;

    public:
        /** Constructor. Zeros out the page data. */
        Page() {
            data_ = new char[BUSTUB_PAGE_SIZE];
            ResetMemory();
        }

        /** Default destructor. */
        ~Page() { delete[] data_; }

        /** @return the actual data contained within this page */
        inline auto GetData() -> char * { return data_; }

        /** @return the page id of this page */
        inline auto GetPageId() -> page_id_t { return page_id_; }

        /** @return the pin count of this page */
        inline auto GetPinCount() -> int { return pin_count_; }

        /** @return true if the page in memory has been modified from the page on
         * disk, false otherwise */
        inline auto IsDirty() -> bool { return is_dirty_; }

        /** Acquire the page write latch. */
        inline void WLatch() { rwlatch_.WLock(); }

        /** Release the page write latch. */
        inline void WUnlatch() { rwlatch_.WUnlock(); }

        /** Acquire the page read latch. */
        inline void RLatch() { rwlatch_.RLock(); }

        /** Release the page read latch. */
        inline void RUnlatch() { rwlatch_.RUnlock(); }

        /** @return the page LSN. */
        inline auto GetLSN() -> lsn_t {
            return *reinterpret_cast<lsn_t *>(GetData() + OFFSET_LSN);
        }

        /** Sets the page LSN. */
        inline void SetLSN(lsn_t lsn) {
            memcpy(GetData() + OFFSET_LSN, &lsn, sizeof(lsn_t));
        }

    protected:
        static_assert(sizeof(page_id_t) == 4);
        static_assert(sizeof(lsn_t) == 4);

        static constexpr size_t SIZE_PAGE_HEADER = 8;
        static constexpr size_t OFFSET_PAGE_START = 0;
        static constexpr size_t OFFSET_LSN = 4;

    private:
        /** Zeroes out the data that is held within the page. */
        inline void ResetMemory() {
            memset(data_, OFFSET_PAGE_START, BUSTUB_PAGE_SIZE);
        }

        /** The actual data that is stored within a page. */
        // Usually this should be stored as `char data_[BUSTUB_PAGE_SIZE]{};`. But to
        // enable ASAN to detect page overflow, we store it as a ptr.
        char *data_;
        /** The ID of this page. */
        page_id_t page_id_ = INVALID_PAGE_ID;
        /** The pin count of this page.
         * 记录页面的钉住计数 。钉住计数用于指示当前页面被 “钉住” 的次数，在缓冲池管理中，
         * 当一个页面被访问且需要保留在内存中时（防止被换出到磁盘 ），
         * 会增加其钉住计数，通过 GetPinCount 方法可获取该计数，初始值为 0 。
         */
        int pin_count_ = 0;
        /** True if the page is dirty, i.e. it is different from its corresponding
         * page on disk.
         * 表示页面是否为脏页 。如果页面在内存中的内容与磁盘上对应的页面内容不同（即被修改过 ），
         * 则该标志为 true ，否则为 false 。可以通过 IsDirty 方法来判断页面是否为脏页，
         * 脏页标志在缓冲池管理和数据库恢复等操作中非常重要，用于决定是否需要将页面写回磁盘 。
         */
        bool is_dirty_ = false;
        /** Page latch.
         * 页面的读写锁 。用于控制对页面的并发访问，通过 WLatch 方法可以获取写锁（独占锁 ），
         * 阻止其他线程对页面进行读写操作；通过 RLatch 方法可以获取读锁（共享锁 ），
         * 允许多个线程同时读取页面，但阻止写操作。WUnlatch 和 RUnlatch 方法分别用于释放写锁和读锁。
         */
        ReaderWriterLatch rwlatch_;
    };
} // namespace bustub
