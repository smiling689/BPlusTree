#pragma once

#include "common/config.h"

namespace bustub {
    /**
     * The header page is just used to retrieve the root page,
     * preventing potential race condition under concurrent environment.
     *
     *  page_id 可以唯一标识一个 page, 它的实现细节你可以忽略。
     *  具体而言， 我们会得到一个函数 (即后文的 NewPageGuarded) 用于 "新建 page, 分配对应 page id 并返回该 page_id"。
     */
    class BPlusTreeHeaderPage {
    public:
        // Delete all constructor / destructor to ensure memory safety
        BPlusTreeHeaderPage() = delete;

        BPlusTreeHeaderPage(const BPlusTreeHeaderPage &other) = delete;

        page_id_t root_page_id_;
    };
} // namespace bustub
