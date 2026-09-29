// Node allocator benchmark for boost::root_ptr, std::unique_ptr, std::shared_ptr and
// boost::shared_node_ptr.
//
//   allocbench <pointer> <allocator> <scenario> [n]
//     pointer:   root unique shared snode
//     allocator: pool fast std page_type page_size
//     scenario:  raw_churn raw_batch churn bulk mixed cycles threads types
//
// Prints "<ns per operation> [<resident kB grown>]", or "n/a" when the scenario
// does not apply to the pointer (or, for threads, to a build with
// BOOST_DISABLE_THREADS). Every scenario runs in a process of its own,
// so memory freed by one scenario is never reused by the next; bench/run.sh
// repeats the runs and reports medians.
//
// Every pointer allocates through the allocator under test:
//   root_ptr    root_ptr<T>(x, new node<T, A<T>>(...))
//   shared_ptr  std::allocate_shared<T>(A<T>(), ...)
//   shared_node_ptr  shared_node_ptr<T>(x, new node<T, A<T>>(...)), as the
//               transformer emits it under FCXXSS_SHARED_PTR: a std::shared_ptr
//               owning the same node root_ptr would
//   unique_ptr  allocate_unique<T>(A<T>(), ...), with a deleter that frees
//               through the same allocator (the standard has no
//               allocator-aware make_unique)
#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/smart_ptr/shared_node_ptr.hpp>
#include <boost/smart_ptr/page_allocator.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <vector>

template <std::size_t N> struct Payload { long v[N / sizeof(long)]; };
typedef Payload<16> Small;
typedef Payload<48> Medium;
typedef Payload<200> Large;

struct Cell
{
    long v;
    boost::root_ptr<Cell> next;
    Cell(boost::node_proxy const & x, long i) : v(i), next(x) { }
};

// ------------------------------------------------------------------ pointers

/// Deleter that destroys and frees through an allocator (for allocate_unique).
template <typename Alloc>
struct alloc_delete : private Alloc     // empty allocators take no space
{
    typedef std::allocator_traits<Alloc> traits;

    alloc_delete() = default;
    explicit alloc_delete(Alloc const & a) : Alloc(a) { }

    void operator () (typename traits::value_type * p)
    {
        Alloc & a = * this;
        traits::destroy(a, p);
        traits::deallocate(a, p, 1);
    }
};

/// The allocator-aware make_unique the standard library does not provide.
template <typename T, typename Alloc, typename... Args>
std::unique_ptr<T, alloc_delete<Alloc>> allocate_unique(Alloc a, Args &&... args)
{
    typedef std::allocator_traits<Alloc> traits;
    T * p = traits::allocate(a, 1);
    traits::construct(a, p, std::forward<Args>(args)...);
    return std::unique_ptr<T, alloc_delete<Alloc>>(p, alloc_delete<Alloc>(a));
}

/// Whether two class templates are the same template.
template <template <typename...> class, template <typename...> class>
struct is_same_template : std::false_type {};

template <template <typename...> class P>
struct is_same_template<P, P> : std::true_type {};

/// The pointer to a T allocated with A, as the pointer type P spells it.
template <template <typename...> class P, template <typename> class A, typename T>
struct pointer_of { typedef P<T> type; };

template <template <typename> class A, typename T>
struct pointer_of<std::unique_ptr, A, T> { typedef std::unique_ptr<T, alloc_delete<A<T>>> type; };

template <template <typename...> class P, template <typename> class A, typename T>
using pointer_t = typename pointer_of<P, A, T>::type;

/// Creates a T through allocator A, owned by pointer type P.
template <template <typename...> class P, template <typename> class A, typename T, typename... Args>
pointer_t<P, A, T> make(boost::node_proxy const & x, Args &&... args)
{
    if constexpr (is_same_template<P, boost::root_ptr>::value)
        return boost::root_ptr<T>(x, new boost::node<T, A<T>>(std::forward<Args>(args)...));
    else if constexpr (is_same_template<P, std::shared_ptr>::value)
        return std::allocate_shared<T>(A<T>(), std::forward<Args>(args)...);
    else if constexpr (is_same_template<P, std::unique_ptr>::value)
        return allocate_unique<T>(A<T>(), std::forward<Args>(args)...);
    else if constexpr (is_same_template<P, boost::shared_node_ptr>::value)
        return boost::shared_node_ptr<T>(x, new boost::node<T, A<T>>(std::forward<Args>(args)...));
    else
        static_assert(! sizeof(T *), "unsupported pointer type");
}

// ------------------------------------------------------------------ measuring

static long resident_kb()
{
    long pages = 0, resident = 0;
    std::ifstream("/proc/self/statm") >> pages >> resident;
    return resident * (sysconf(_SC_PAGESIZE) / 1024);
}

template <typename F>
static double ns_per_op(long ops, F && f)
{
    auto t0 = std::chrono::steady_clock::now();
    f();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / ops;
}

/// Makes the compiler assume the object at @p p is used, so that it cannot
/// elide an allocation whose pointer would otherwise never escape (clang
/// removed whole new/delete pairs for unique_ptr + std::allocator).
static inline void escape(void const * p)
{
    asm volatile("" : : "g"(p) : "memory");
}

// 'types': many distinct types of the same size, a few objects of each - where
// type-oriented pages (one page per type) and size-oriented pages (one page per
// size class) differ. Each level keeps its own vector alive while the next
// level builds, so all 32 types are live when the deepest level measures.
template <int I> struct Tagged { long v[6]; };

template <template <typename...> class P, template <typename> class A, int I>
struct many_types
{
    static long build(boost::node_proxy const & x, long n, long rss0)
    {
        std::vector<pointer_t<P, A, Tagged<I>>> v;
        v.reserve(n);
        for (long i = 0; i < n; ++i)
            v.push_back(make<P, A, Tagged<I>>(x));
        return many_types<P, A, I - 1>::build(x, n, rss0);
    }
};

template <template <typename...> class P, template <typename> class A>
struct many_types<P, A, 0>
{
    static long build(boost::node_proxy const &, long, long rss0) { return resident_kb() - rss0; }
};

// ------------------------------------------------------------------ scenarios

template <template <typename...> class P, template <typename> class A>
static bool run(std::string const & scenario, long arg)
{
    constexpr bool is_root = is_same_template<P, boost::root_ptr>::value;
    auto size = [&](long d) { return arg > 0 ? arg : d; };

    // raw: the allocator alone, one node-sized block at a time (no pointer involved)
    typedef typename boost::node<Medium, A<Medium>>::allocator_type R;
    if (scenario == "raw_churn")
    {
        R a;
        const long n = size(4000000);
        std::printf("%.2f\n", ns_per_op(n, [&] {
            for (long i = 0; i < n; ++i)
            {
                auto * p = a.allocate(1);
                escape(p);
                a.deallocate(p, 1);
            }
        }));
    }
    // batch: allocate m blocks, then free them in allocation order
    else if (scenario == "raw_batch")
    {
        R a;
        const long m = size(100000);
        std::vector<typename std::allocator_traits<R>::pointer> v(m);
        std::printf("%.2f\n", ns_per_op(2 * m, [&] {
            for (long i = 0; i < m; ++i) v[i] = a.allocate(1);
            for (long i = 0; i < m; ++i) a.deallocate(v[i], 1);
        }));
    }
    // churn: one object created and dropped per iteration
    else if (scenario == "churn")
    {
        boost::node_proxy x(__FILE__, __func__, __LINE__);
        const long n = size(1000000);
        std::printf("%.2f\n", ns_per_op(n, [&] {
            for (long i = 0; i < n; ++i)
            {
                auto p = make<P, A, Medium>(x);
                escape(&*p);
            }
        }));
    }
    // bulk: n live objects, all released together at the end of the scope
    else if (scenario == "bulk")
    {
        const long n = size(100000);
        long rss0 = resident_kb(), grown = 0;
        double t = ns_per_op(n, [&] {
            boost::node_proxy x(__FILE__, __func__, __LINE__);
            std::vector<pointer_t<P, A, Medium>> v;
            v.reserve(n);
            for (long i = 0; i < n; ++i)
                v.push_back(make<P, A, Medium>(x));
            grown = resident_kb() - rss0;
        });
        std::printf("%.2f %ld\n", t, grown);
    }
    // mixed: three payload sizes interleaved, n of each
    else if (scenario == "mixed")
    {
        const long n = size(50000);
        long rss0 = resident_kb(), grown = 0;
        double t = ns_per_op(3 * n, [&] {
            boost::node_proxy x(__FILE__, __func__, __LINE__);
            std::vector<pointer_t<P, A, Small>> s;
            std::vector<pointer_t<P, A, Medium>> m;
            std::vector<pointer_t<P, A, Large>> l;
            s.reserve(n); m.reserve(n); l.reserve(n);
            for (long i = 0; i < n; ++i)
            {
                s.push_back(make<P, A, Small>(x));
                m.push_back(make<P, A, Medium>(x));
                l.push_back(make<P, A, Large>(x));
            }
            grown = resident_kb() - rss0;
        });
        std::printf("%.2f %ld\n", t, grown);
    }
    // cycles: n two-node cycles, reclaimed with their proxy. Only root_ptr can:
    // a unique_ptr cannot form a cycle and a shared_ptr (or shared_node_ptr)
    // cycle is never freed.
    else if (scenario == "cycles")
    {
        if constexpr (is_root)
        {
            const long n = size(25000);
            std::printf("%.2f\n", ns_per_op(2 * n, [&] {
                boost::node_proxy x(__FILE__, __func__, __LINE__);
                for (long i = 0; i < n; ++i)
                {
                    auto a = make<P, A, Cell>(x, x, i);
                    auto b = make<P, A, Cell>(x, x, i);
                    a->next = b;
                    b->next = a;
                }
            }));
        }
        else
            std::printf("n/a\n");
    }
    // threads: 4 threads, each churning objects (in its own proxy for root_ptr).
    // Not in a build without thread support (BOOST_DISABLE_THREADS): nothing is locked.
    else if (scenario == "threads")
    {
#if ! defined(BOOST_HAS_THREADS)
        std::printf("n/a\n");
        return true;
#endif
        const long n = size(250000);
        const int threads = 4;
        std::printf("%.2f\n", ns_per_op(threads * n, [&] {
            std::vector<std::thread> t;
            for (int k = 0; k < threads; ++k)
                t.emplace_back([&] {
                    boost::node_proxy x(__FILE__, __func__, __LINE__);
                    for (long i = 0; i < n; ++i)
                    {
                        auto p = make<P, A, Medium>(x);
                        escape(&*p);
                    }
                });
            for (auto & th : t) th.join();
        }));
    }
    // types: 32 types of equal size, n live objects of each
    else if (scenario == "types")
    {
        const long n = size(100);
        long rss0 = resident_kb(), grown = 0;
        double t = ns_per_op(32 * n, [&] {
            boost::node_proxy x(__FILE__, __func__, __LINE__);
            grown = many_types<P, A, 32>::build(x, n, rss0);
        });
        std::printf("%.2f %ld\n", t, grown);
    }
    else
        return false;
    return true;
}

template <typename T> using pool_a = boost::pool_allocator<T>;
template <typename T> using fast_a = boost::fast_pool_allocator<T>;
template <typename T> using std_a = std::allocator<T>;
template <typename T> using page_type_a = boost::page_allocator_by_type<T>;
template <typename T> using page_size_a = boost::page_allocator_by_size<T>;

template <template <typename...> class P>
static bool run_allocator(std::string const & a, std::string const & s, long n)
{
    return a == "pool"      ? run<P, pool_a>(s, n)
         : a == "fast"      ? run<P, fast_a>(s, n)
         : a == "std"       ? run<P, std_a>(s, n)
         : a == "page_type" ? run<P, page_type_a>(s, n)
         : a == "page_size" ? run<P, page_size_a>(s, n)
         : false;
}

int main(int argc, char ** argv)
{
    if (argc < 4) { std::fprintf(stderr, "usage: allocbench <pointer> <allocator> <scenario> [n]\n"); return 1; }
    std::string p = argv[1], a = argv[2], s = argv[3];
    long n = argc > 4 ? std::atol(argv[4]) : 0;
    bool ok = p == "root"   ? run_allocator<boost::root_ptr>(a, s, n)
            : p == "unique" ? run_allocator<std::unique_ptr>(a, s, n)
            : p == "shared" ? run_allocator<std::shared_ptr>(a, s, n)
            : p == "snode"  ? run_allocator<boost::shared_node_ptr>(a, s, n)
            : false;
    if (! ok)
    {
        std::fprintf(stderr, "unknown pointer, allocator or scenario: %s %s %s\n", p.c_str(), a.c_str(), s.c_str());
        return 1;
    }
    return 0;
}
