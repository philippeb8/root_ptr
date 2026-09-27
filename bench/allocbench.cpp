// Node allocator benchmark for boost::root_ptr.
//
//   allocbench <allocator> <scenario> [n]
//     allocator: pool fast std page_type page_size
//     scenario:  raw_churn raw_batch root_churn bulk mixed cycles threads types
//
// Prints "<ns per operation> [<resident kB grown>]". Every scenario runs in a
// process of its own, so memory freed by one scenario is never reused by the
// next; bench/run.sh repeats the runs and reports medians.
#include <boost/smart_ptr/root_ptr.hpp>
#include <boost/smart_ptr/page_allocator.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
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

static volatile long sink;

// 'types': many distinct types of the same size, a few nodes of each - where
// type-oriented pages (one page per type) and size-oriented pages (one page per
// size class) differ.
template <int I> struct Tagged { long v[6]; };

template <template <typename> class A, int I>
struct many_types
{
    static void build(boost::node_proxy const & x, long n, std::vector<boost::root_ptr<void>> & keep)
    {
        for (long i = 0; i < n; ++i)
            keep.emplace_back(boost::root_ptr<Tagged<I>>(x, new boost::node<Tagged<I>, A<Tagged<I>>>()));
        many_types<A, I - 1>::build(x, n, keep);
    }
};

template <template <typename> class A>
struct many_types<A, 0>
{
    static void build(boost::node_proxy const &, long, std::vector<boost::root_ptr<void>> &) { }
};

template <template <typename> class A>
static bool run(std::string const & scenario, long arg)
{
    auto size = [&](long d) { return arg > 0 ? arg : d; };

    // raw: the allocator alone, one node-sized block at a time
    typedef typename boost::node<Medium, A<Medium>>::allocator_type R;
    if (scenario == "raw_churn")
    {
        R a;
        const long n = size(4000000);
        std::printf("%.2f\n", ns_per_op(n, [&] {
            for (long i = 0; i < n; ++i)
            {
                auto * p = a.allocate(1);
                sink = reinterpret_cast<long>(p);
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
    // churn: one node created and dropped per iteration, through root_ptr
    else if (scenario == "root_churn")
    {
        boost::node_proxy x(__FILE__, __func__, __LINE__);
        const long n = size(1000000);
        std::printf("%.2f\n", ns_per_op(n, [&] {
            for (long i = 0; i < n; ++i)
            {
                boost::root_ptr<Medium> p(x, new boost::node<Medium, A<Medium>>());
                sink = p->v[0];
            }
        }));
    }

    // bulk: n live nodes, all released when the proxy goes out of scope
    else if (scenario == "bulk")
    {
        const long n = size(100000);
        long rss0 = resident_kb(), grown = 0;
        double t = ns_per_op(n, [&] {
            boost::node_proxy x(__FILE__, __func__, __LINE__);
            std::vector<boost::root_ptr<Medium>> v;
            v.reserve(n);
            for (long i = 0; i < n; ++i)
                v.emplace_back(x, new boost::node<Medium, A<Medium>>());
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
            std::vector<boost::root_ptr<Small>> s;
            std::vector<boost::root_ptr<Medium>> m;
            std::vector<boost::root_ptr<Large>> l;
            s.reserve(n); m.reserve(n); l.reserve(n);
            for (long i = 0; i < n; ++i)
            {
                s.emplace_back(x, new boost::node<Small, A<Small>>());
                m.emplace_back(x, new boost::node<Medium, A<Medium>>());
                l.emplace_back(x, new boost::node<Large, A<Large>>());
            }
            grown = resident_kb() - rss0;
        });
        std::printf("%.2f %ld\n", t, grown);
    }
    // cycles: n two-node cycles, reclaimed with their proxy
    else if (scenario == "cycles")
    {
        const long n = size(25000);
        std::printf("%.2f\n", ns_per_op(2 * n, [&] {
            boost::node_proxy x(__FILE__, __func__, __LINE__);
            for (long i = 0; i < n; ++i)
            {
                boost::root_ptr<Cell> a(x, new boost::node<Cell, A<Cell>>(x, i));
                boost::root_ptr<Cell> b(x, new boost::node<Cell, A<Cell>>(x, i));
                a->next = b;
                b->next = a;
            }
        }));
    }

    // threads: 4 threads, each churning nodes in its own proxy
    else if (scenario == "threads")
    {
        const long n = size(250000);
        const int threads = 4;
        std::printf("%.2f\n", ns_per_op(threads * n, [&] {
            std::vector<std::thread> t;
            for (int k = 0; k < threads; ++k)
                t.emplace_back([&] {
                    boost::node_proxy x(__FILE__, __func__, __LINE__);
                    for (long i = 0; i < n; ++i)
                    {
                        boost::root_ptr<Medium> p(x, new boost::node<Medium, A<Medium>>());
                        sink = p->v[0];
                    }
                });
            for (auto & th : t) th.join();
        }));
    }
    // types: 32 types of equal size, n live nodes of each
    else if (scenario == "types")
    {
        const long n = size(100);
        long rss0 = resident_kb(), grown = 0;
        double t = ns_per_op(32 * n, [&] {
            boost::node_proxy x(__FILE__, __func__, __LINE__);
            std::vector<boost::root_ptr<void>> keep;
            keep.reserve(32 * n);
            many_types<A, 32>::build(x, n, keep);
            grown = resident_kb() - rss0;
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

int main(int argc, char ** argv)
{
    if (argc < 3) { std::fprintf(stderr, "usage: allocbench <allocator> <scenario> [n]\n"); return 1; }
    std::string a = argv[1], s = argv[2];
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    bool ok = a == "pool"      ? run<pool_a>(s, n)
            : a == "fast"      ? run<fast_a>(s, n)
            : a == "std"       ? run<std_a>(s, n)
            : a == "page_type" ? run<page_type_a>(s, n)
            : a == "page_size" ? run<page_size_a>(s, n)
            : false;
    if (! ok) { std::fprintf(stderr, "unknown allocator or scenario: %s %s\n", a.c_str(), s.c_str()); return 1; }
    return 0;
}
