/**
    @file
    @brief Page allocator whose pages hold blocks of one type or of one size.

    Patent US11288049B2
    'SOURCE TO SOURCE COMPILER, COMPILATION METHOD, AND
    COMPUTER-READABLE MEDIUM FOR PREDICTABLE MEMORY MANAGEMENT'

    @copyright Copyright (C) 2020-2026 Services Informatiques Fornux

    @author Phil Bouchard, Founder & CEO
    Services Informatiques Fornux
    phil@fornux.com
    20 Poirier St., Gatineau, Quebec, Canada, J8V 1A6

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef BOOST_SMART_PTR_PAGE_ALLOCATOR_HPP_INCLUDED
#define BOOST_SMART_PTR_PAGE_ALLOCATOR_HPP_INCLUDED

#include <boost/config.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>


namespace boost
{

/** @brief Page selection policy: every type gets pages of its own. */
struct by_type {};

/** @brief Page selection policy: every block size class gets pages of its own, shared by all types in it. */
struct by_size {};


namespace smart_ptr
{
namespace detail
{

/**
    @brief Mutex of every pool.

    A no-op when Boost is configured without thread support
    (@c BOOST_DISABLE_THREADS), as for Boost.Pool and root_ptr itself.
*/
#if defined(BOOST_HAS_THREADS)
typedef std::mutex page_mutex;
#else
struct page_mutex
{
    void lock() noexcept { }
    void unlock() noexcept { }
};
#endif

/**
    @brief Per-thread caches of free blocks, opt-in with
    @c BOOST_PAGE_ALLOCATOR_THREAD_CACHE; ignored without thread support.
*/
#if defined(BOOST_HAS_THREADS) && defined(BOOST_PAGE_ALLOCATOR_THREAD_CACHE)
#define BOOST_PAGE_ALLOCATOR_CACHE_ACTIVE
#endif

/// Alignment of every block and page.
constexpr std::size_t page_alignment = alignof(std::max_align_t);

/** @brief Rounds @p n up to a multiple of @p a, a power of two. */
constexpr std::size_t round_up(std::size_t n, std::size_t a)
{
    return (n + a - 1) & ~(a - 1);
}

/**
    @brief Size class of a block of @p n bytes.

    Multiples of 16 bytes up to 256, then powers of two, so that at most about
    25% of a block is wasted.
*/
constexpr std::size_t size_class(std::size_t n)
{
    if (n <= 256)
        return round_up(n ? n : 1, 16);

    std::size_t c = 512;
    while (c < n)
        c <<= 1;
    return c;
}

/**
    @brief Statistics of one pool.
*/
struct page_pool_stats
{
    std::size_t block_size = 0;     ///< Size of every block, in bytes.
    std::size_t pages = 0;          ///< Pages obtained so far; they are never returned.
    std::size_t blocks_per_page = 0;///< Blocks carved from each page.
    std::size_t live = 0;           ///< Blocks currently allocated.
    std::size_t cached = 0;         ///< Free blocks held by thread caches (@c BOOST_PAGE_ALLOCATOR_THREAD_CACHE).
};

/**
    @brief Pool of equally sized blocks carved from pages of @c PageSize bytes.

    Free blocks form a singly linked list threaded through the blocks
    themselves, so allocation and deallocation are O(1) and need no header.
    @c Tag separates pools that would otherwise share a block size.

    With @c BOOST_PAGE_ALLOCATOR_THREAD_CACHE (and thread support), every
    thread that allocates keeps a small stack of free blocks per pool:
    allocation and deallocation take no lock until the stack runs empty or
    grows past its cap, and then move a batch at once. Blocks are
    interchangeable, so a block may be freed by any thread. A thread returns
    its blocks when it exits; a thread that never allocated from a pool frees
    into the shared list directly.

    Instances are created on first use and never destroyed: a node may be
    released during static destruction, after a static pool would be gone.
*/
template <std::size_t BlockSize, std::size_t PageSize, typename Tag>
    class page_pool
    {
        static_assert(BlockSize % page_alignment == 0, "block size must keep the alignment");
        static_assert(PageSize >= BlockSize, "a page must hold at least one block");

        struct free_block { free_block * next; };

        page_mutex mutex_;
        free_block * free_ = nullptr;
        page_pool_stats stats_;         // with thread caches, 'live' counts every block off the shared list

        page_pool()
        {
            stats_.block_size = BlockSize;
            stats_.blocks_per_page = PageSize / BlockSize;
        }

        /// Carves a new page into blocks and pushes them on the free list.
        void grow()
        {
            char * page = static_cast<char *>(::operator new(PageSize, std::align_val_t(page_alignment)));

            for (std::size_t i = stats_.blocks_per_page; i-- > 0;)
            {
                free_block * b = reinterpret_cast<free_block *>(page + i * BlockSize);
                b->next = free_;
                free_ = b;
            }
            ++ stats_.pages;
        }

        /// Takes one block from the shared list; the mutex is held.
        free_block * pop_shared()
        {
            if (! free_)
                grow();

            free_block * b = free_;
            free_ = b->next;
            ++ stats_.live;
            return b;
        }

        /// Returns one block to the shared list.
        void push_shared(void * p) noexcept
        {
            std::lock_guard<page_mutex> guard(mutex_);

            free_block * b = static_cast<free_block *>(p);
            b->next = free_;
            free_ = b;
            -- stats_.live;
        }

#if defined(BOOST_PAGE_ALLOCATOR_CACHE_ACTIVE)
        /// Blocks a thread keeps: 16 KiB worth, at least 8 and at most 256.
        static constexpr std::size_t cache_max = 16 * 1024 / BlockSize < 8 ? 8 : 16 * 1024 / BlockSize > 256 ? 256 : 16 * 1024 / BlockSize;

        /// Blocks moved between a thread's stack and the shared list at once.
        static constexpr std::size_t cache_batch = cache_max / 2;

        enum cache_state : unsigned char { unused, active, retired };

        /**
            @brief A thread's free blocks of this pool.

            Constant-initialized and trivially destructible, so it stays usable
            after the thread's other thread_local objects are destroyed.
        */
        struct thread_cache
        {
            free_block * head = nullptr;
            std::atomic<std::size_t> count{0};  ///< Written by the owner only; read by stats().
            cache_state state = unused;
            thread_cache * prev = nullptr;      ///< Registry of the pool, under its mutex.
            thread_cache * next = nullptr;
        };

        /// Returns the thread's blocks when the thread exits.
        struct cache_flusher
        {
            ~cache_flusher() { instance().retire(local()); }
        };

        thread_cache * caches_ = nullptr;

        static thread_cache & local() noexcept
        {
            static thread_local thread_cache c;

            return c;
        }

        /// First allocation of this thread: registers its cache and its flusher.
        void enroll(thread_cache & c)
        {
            static thread_local cache_flusher flusher;
            (void) flusher;

            std::lock_guard<page_mutex> guard(mutex_);

            c.next = caches_;
            if (caches_)
                caches_->prev = & c;
            caches_ = & c;
            c.state = active;
        }

        /// Refills the empty cache of this thread with a batch.
        void refill(thread_cache & c)
        {
            std::lock_guard<page_mutex> guard(mutex_);

            for (std::size_t i = 0; i < cache_batch; ++ i)
            {
                free_block * b = pop_shared();
                b->next = c.head;
                c.head = b;
            }
            c.count.store(cache_batch, std::memory_order_relaxed);
        }

        /// Returns the first @p n blocks of this thread's cache to the shared list.
        void drain(thread_cache & c, std::size_t n) noexcept
        {
            free_block * first = c.head, * last = first;

            for (std::size_t i = 1; i < n; ++ i)
                last = last->next;
            c.head = last->next;
            c.count.store(c.count.load(std::memory_order_relaxed) - n, std::memory_order_relaxed);

            std::lock_guard<page_mutex> guard(mutex_);

            last->next = free_;
            free_ = first;
            stats_.live -= n;
        }

        /// The thread exits: returns every block and leaves the registry.
        void retire(thread_cache & c) noexcept
        {
            if (std::size_t n = c.count.load(std::memory_order_relaxed))
                drain(c, n);

            std::lock_guard<page_mutex> guard(mutex_);

            if (c.prev)
                c.prev->next = c.next;
            else
                caches_ = c.next;
            if (c.next)
                c.next->prev = c.prev;
            c.prev = c.next = nullptr;
            c.state = retired;
        }
#endif

    public:
        page_pool(page_pool const &) = delete;
        page_pool & operator = (page_pool const &) = delete;

        /** @brief The one pool for this block size, page size and tag. */
        static page_pool & instance()
        {
            static page_pool * p = new page_pool();

            return * p;
        }

        /** @brief Allocates one block. */
        void * allocate()
        {
#if defined(BOOST_PAGE_ALLOCATOR_CACHE_ACTIVE)
            thread_cache & c = local();

            if (! c.head)
            {
                if (c.state == retired)
                {
                    std::lock_guard<page_mutex> guard(mutex_);

                    return pop_shared();
                }
                if (c.state == unused)
                    enroll(c);
                refill(c);
            }

            free_block * b = c.head;
            c.head = b->next;
            c.count.store(c.count.load(std::memory_order_relaxed) - 1, std::memory_order_relaxed);
            return b;
#else
            std::lock_guard<page_mutex> guard(mutex_);

            return pop_shared();
#endif
        }

        /** @brief Returns a block obtained from allocate(), by any thread. */
        void deallocate(void * p) noexcept
        {
#if defined(BOOST_PAGE_ALLOCATOR_CACHE_ACTIVE)
            thread_cache & c = local();

            if (c.state != active)
                return push_shared(p);

            free_block * b = static_cast<free_block *>(p);
            b->next = c.head;
            c.head = b;

            std::size_t n = c.count.load(std::memory_order_relaxed) + 1;
            c.count.store(n, std::memory_order_relaxed);
            if (n >= cache_max)
                drain(c, cache_batch);
#else
            push_shared(p);
#endif
        }

        /**
            @brief Snapshot of the statistics.

            Blocks held by thread caches count as free (@c cached), not as
            live. The snapshot is exact only while no thread allocates.
        */
        page_pool_stats stats()
        {
            std::lock_guard<page_mutex> guard(mutex_);

            page_pool_stats s = stats_;
#if defined(BOOST_PAGE_ALLOCATOR_CACHE_ACTIVE)
            for (thread_cache * c = caches_; c; c = c->next)
                s.cached += c->count.load(std::memory_order_relaxed);
            s.live -= s.cached;
#endif
            return s;
        }
    };

/** @brief The pool serving @c T under @c Policy. */
template <typename T, typename Policy, std::size_t PageSize>
    struct pool_of;

template <typename T, std::size_t PageSize>
    struct pool_of<T, by_type, PageSize>
    {
        typedef page_pool<round_up(sizeof(T), page_alignment), PageSize, T> type;
    };

template <typename T, std::size_t PageSize>
    struct pool_of<T, by_size, PageSize>
    {
        typedef page_pool<round_up(size_class(sizeof(T)), page_alignment), PageSize, void> type;
    };

} // namespace detail
} // namespace smart_ptr


/**
    @brief Allocator whose memory pages hold blocks of one type or of one size.

    With @c by_type, every type has its own pages; with @c by_size, types whose
    size falls in the same size class share pages. Single objects come from the
    pool; arrays, and blocks larger than an eighth of a page, come from
    @c ::operator new. The allocator is stateless: all instances are equal.

    @tparam T        Type of the objects allocated.
    @tparam Policy   @c by_type or @c by_size.
    @tparam PageSize Size of every page, in bytes.
*/
template <typename T, typename Policy = by_size, std::size_t PageSize = 64 * 1024>
    class page_allocator
    {
        // Both are only evaluated inside the member functions: the allocator is
        // instantiated as a member of node<T>, while node<T> is still incomplete.

        /// Pool serving single objects of T.
        template <typename U = T>
            using pool_type = typename smart_ptr::detail::pool_of<U, Policy, PageSize>::type;

        /// Whether single objects of T come from a pool.
        template <typename U = T>
            static constexpr bool pooled()
            {
                return alignof(U) <= smart_ptr::detail::page_alignment && sizeof(U) <= PageSize / 8;
            }

    public:
        typedef T value_type;
        typedef std::true_type is_always_equal;

        /** @brief The same allocator for another type. */
        template <typename U>
            struct rebind
            {
                typedef page_allocator<U, Policy, PageSize> other;
            };

        page_allocator() noexcept = default;

        template <typename U>
            page_allocator(page_allocator<U, Policy, PageSize> const &) noexcept
            {
            }

        /** @brief Allocates @p n objects of @c T. */
        T * allocate(std::size_t n)
        {
            if constexpr (pooled())
                if (n == 1)
                    return static_cast<T *>(pool_type<>::instance().allocate());

            return static_cast<T *>(::operator new(n * sizeof(T), std::align_val_t(alignof(T))));
        }

        /** @brief Frees @p n objects obtained from allocate(n). */
        void deallocate(T * p, std::size_t n) noexcept
        {
            if constexpr (pooled())
                if (n == 1)
                    return pool_type<>::instance().deallocate(p);

            ::operator delete(p, std::align_val_t(alignof(T)));
        }

        /** @brief Statistics of the pool serving single objects of @c T. */
        static smart_ptr::detail::page_pool_stats stats()
        {
            if constexpr (pooled())
                return pool_type<>::instance().stats();
            else
                return smart_ptr::detail::page_pool_stats();
        }

        template <typename U>
            bool operator == (page_allocator<U, Policy, PageSize> const &) const noexcept
            {
                return true;
            }

        template <typename U>
            bool operator != (page_allocator<U, Policy, PageSize> const &) const noexcept
            {
                return false;
            }
    };

/// Page allocator whose pages each hold objects of a single type.
template <typename T>
    using page_allocator_by_type = page_allocator<T, by_type>;

/// Page allocator whose pages each hold blocks of a single size class.
template <typename T>
    using page_allocator_by_size = page_allocator<T, by_size>;

} // namespace boost

#endif // #ifndef BOOST_SMART_PTR_PAGE_ALLOCATOR_HPP_INCLUDED
