# Node allocator benchmark: page allocators vs. the default pool

Measured 2026-09-26/27 with `bench/allocbench.cpp` and `bench/run.sh`.

## Summary

- **The default allocator, `boost::pool_allocator`, is quadratic when many nodes are released.**
  Its `deallocate` calls `ordered_free`, which walks the free list to keep it sorted
  (`simple_segregated_storage::find_prev` in Boost.Pool). Releasing 200,000 nodes at once costs
  354 µs per node, about 71 s in total; the per-node cost grows ×1.7 to ×2.4 every time the node count
  doubles. Every other allocator stays near 100 ns per node at every size.
- **`page_allocator_by_size` is the fastest or tied for fastest in every single-threaded scenario.**
  It is 2.5× faster than the default on allocate/free churn, 1.3× on `root_ptr` churn, and
  570× to 2,950× faster on batch, bulk, mixed-size and cycle release. It also holds the same nodes
  in 9–15% less memory.
- **`page_allocator_by_type` matches `by_size` when there are few types, but costs more with many
  sparse ones.** 32 types of 100 nodes each take 336 ns per node and 3.8 MB, against 128 ns and 2.2 MB
  for `by_size`. Each type fills a 64 KiB page of its own, and a new page is fully touched when its
  free list is built.
- **With four threads, `std::allocator` is fastest** (284 ns per node, against 386–391 ns for the page
  allocators and 441 ns for the default). glibc gives each thread its own arena, while every page pool
  has one mutex; `root_ptr` itself serializes on a global mutex, which narrows the gap.

## Setup

| | |
|---|---|
| CPU | Intel Core i7-4700HQ, 4 cores / 8 threads, 2.4 GHz |
| Memory | 16 GB |
| OS | Linux 5.15 (Ubuntu 22.04) |
| Compiler | clang 23 (`-std=c++20 -O2 -DNDEBUG`), libstdc++ 12, Boost.Pool from `/usr/include` |
| Library | `/opt/fornux/superset/usr/include` (root_ptr.hpp with the 2026-09-26 allocator fixes) |
| Load | Shared desktop machine (browser, Xorg); load average 0.8 at the start, up to 5 during the threaded runs |

Allocators, each used as `boost::node<T, A>`:

| name | allocator |
|---|---|
| `pool` | `boost::pool_allocator` (the default) |
| `fast` | `boost::fast_pool_allocator` |
| `std` | `std::allocator` (glibc `malloc`) |
| `page_type` | `boost::page_allocator_by_type` (pages hold one type) |
| `page_size` | `boost::page_allocator_by_size` (pages hold one size class) |

Scenarios. "Medium" is a 48-byte payload; every node also carries the `node` header.

| scenario | what one operation is |
|---|---|
| `raw_churn` | allocator only: allocate one node-sized block and free it at once (4,000,000 times) |
| `raw_batch` | allocator only: allocate 100,000 blocks, then free them in allocation order |
| `root_churn` | create a `root_ptr` to a new Medium node and drop it (1,000,000 times) |
| `bulk` | 100,000 live Medium nodes, all released when their `node_proxy` ends |
| `mixed` | 50,000 each of 16-, 48- and 200-byte payloads interleaved, released together |
| `cycles` | 25,000 two-node cycles, reclaimed with their `node_proxy` |
| `threads` | 4 threads, each creating and dropping 250,000 nodes in its own `node_proxy` |
| `types` | 32 distinct types of equal size, 100 live nodes of each, released together |

Method: every (allocator, scenario) pair runs in a fresh process, so memory freed by one scenario is
never reused by the next. The main table is the median of 5 runs, interleaved across allocators so
that drift in machine load hits all of them alike. Time is wall clock per operation, including the
release of every node. Memory is the growth of resident set size (RSS, from `/proc/self/statm`) while
the nodes are live, including the process's first-use costs, which `std` shows as a floor.

## Results

Time, ns per operation; the speed-up over `pool` is in parentheses:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 40.9 (1.00x) | 17.1 (2.40x) | 15.0 (2.73x) | 16.1 (2.54x) | 16.5 (2.48x) |
| raw_batch | 69020.1 (1.00x) | 25.8 (2674.16x) | 27.7 (2495.31x) | 23.7 (2913.47x) | 23.4 (2949.58x) |
| root_churn | 75.3 (1.00x) | 56.8 (1.33x) | 53.7 (1.40x) | 55.7 (1.35x) | 56.5 (1.33x) |
| bulk | 138548.3 (1.00x) | 99.7 (1389.93x) | 105.0 (1318.88x) | 97.2 (1426.13x) | 96.3 (1439.16x) |
| mixed | 93292.2 (1.00x) | 126.1 (739.71x) | 145.1 (643.04x) | 118.2 (789.07x) | 118.5 (787.34x) |
| cycles | 68311.7 (1.00x) | 125.6 (543.75x) | 132.2 (516.61x) | 117.7 (580.39x) | 119.0 (573.90x) |
| threads | 441.2 (1.00x) | 391.4 (1.13x) | 283.8 (1.55x) | 386.2 (1.14x) | 391.5 (1.13x) |
| types | 6717.8 (1.00x) | 134.5 (49.96x) | 146.0 (46.03x) | 335.5 (20.02x) | 128.0 (52.50x) |

Resident memory grown while the nodes are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13840 | 13888 | 12528 | 12560 | 12568 |
| mixed | 27880 | 27884 | 24184 | 23664 | 23732 |
| types | 2120 | 2148 | 1908 | 3772 | 2152 |

Bulk release scaling, ns per node, one run per size:

| nodes | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 21065.9 | 111.0 | 111.1 | 107.7 | 93.4 |
| 25000 | 36563.9 | 105.8 | 109.6 | 106.1 | 97.6 |
| 50000 | 69750.7 | 101.0 | 104.0 | 96.3 | 92.4 |
| 100000 | 150551.1 | 105.2 | 108.5 | 106.8 | 102.2 |
| 200000 | 354043.6 | 100.6 | 103.5 | 94.2 | 96.5 |

## Analysis

**The default pool.** `boost::pool_allocator` keeps its free list sorted by address, so each
`deallocate` is a linear search. That doesn't show when one node is allocated and freed at a time
(`raw_churn`, `root_churn`: the list stays short), but every scenario that releases many nodes together
pays per node in proportion to the number already free. In the scaling table the default's per-node
cost goes from 21 µs to 354 µs, ×1.7 to ×2.4 for each doubling, while the others stay flat. Releasing a
`node_proxy` scope with many nodes is exactly what transformed programs do, so this affects them
directly. The gap between `raw_batch` and `bulk` is the `root_ptr` and node bookkeeping.

**Page allocators vs. `fast_pool_allocator`.** Both are segregated free lists with O(1) allocate and
free, so the times are close; the page allocators are within a few percent of `fast` and slightly
ahead in `bulk`, `mixed` and `cycles`. They use 9–15% less memory than `pool` and `fast` for the same
nodes. The Boost pools grow in doubling chunks, so part of their last chunk is reserved but unused;
the page allocators grow one 64 KiB page at a time.

**By type vs. by size.** With one or three types (`bulk`, `mixed`) the two policies behave the same,
because each type has its own size class anyway. With many types of the same size, `by_type` opens one
64 KiB page per type. Building a page's free list writes into every block, so the whole page becomes
resident even if it holds only a handful of nodes: +1.6 MB and 2.6× the time of `by_size` in `types`.
`by_type` pays off only when keeping types apart matters more than footprint, for example to confine
a use-after-free of one type to that type's pages.

**Threads.** glibc's per-thread arenas make `std::allocator` the fastest under contention. The page
allocators take one mutex per pool, and all four threads allocate the same type, so they share a
pool. The gap stays modest (1.36×) because `root_ptr` operations already serialize on
`static_recursive_mutex()`.

## Recommendations

1. **Change the default node allocator.** `page_allocator_by_size` or `fast_pool_allocator` removes the
   quadratic release at no cost in any single-threaded scenario measured here. This is a library
   default, so it's the maintainer's decision; `-D BOOST_ROOT_PTR_ALLOCATOR=...` already selects one per
   build.
2. **Carve pages lazily.** `page_pool::grow()` threads the free list through a whole page up front,
   touching all of it. Handing out blocks from a bump pointer and only threading freed blocks would
   make a sparsely used page cost only what it holds, which is what hurts `page_allocator_by_type`.
3. **For heavily threaded programs**, per-thread caches in front of the page pools would close the gap
   to `std::allocator`; `root_ptr`'s global mutex limits the benefit.

## Reproducing

```sh
bench/run.sh 5        # main tables and scaling; the default pool dominates the run time (about 7 minutes)
# a single measurement:
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    bench/allocbench.cpp -o allocbench -lboost_thread -lpthread
./allocbench page_size types     # <allocator> <scenario> [n]
```

The `types` row was measured separately with the same method (5 interleaved runs, medians), after
the scenario was added. `run.sh` includes it from now on. The raw output of the main run is in
`bench/bench-run.log`.
