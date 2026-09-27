# Node allocator benchmark: page allocators vs. the default pool

Measured 2026-09-27 with `bench/allocbench.cpp` and `bench/run.sh`, for three smart pointer types:
`boost::root_ptr`, `std::unique_ptr` and `std::shared_ptr`.

## Summary

- **The default allocator, `boost::pool_allocator`, is quadratic when many objects are released.**
  Its `deallocate` calls `ordered_free`, which walks the free list to keep it sorted
  (`simple_segregated_storage::find_prev` in Boost.Pool). This does not depend on the pointer type:
  releasing 200,000 objects at once costs 253–436 µs per object with the default pool, and the cost per
  object grows ×1.7 to ×2.7 each time the count doubles. Every other allocator stays flat, at 40–160 ns
  per object at every size, apart from single-run outliers.
- **`page_allocator_by_size` is the fastest, or within 5% of the fastest, in every single-threaded
  scenario for all three pointer types, except churn with `unique_ptr` and `shared_ptr`,** where
  `std::allocator` (glibc's per-thread cache) is 12–17% faster. Compared with the default pool, it is
  1.4× to 3.2× faster on churn, 570× to 2,400× faster on bulk, mixed-size and cycle release, and it holds
  the same objects in 9–18% less memory.
- **`page_allocator_by_type` matches `by_size` with few types, but costs more with many sparse ones.**
  32 types of 100 objects each take 2.6× to 4× the time of `by_size` and 1.6–1.9 MB more memory.
  Each type fills a 64 KiB page of its own, and a new page is fully touched when its free list is built.
- **With four threads, `std::allocator` is fastest by far for `unique_ptr` and `shared_ptr`**
  (4–10 ns per object, against 157–188 ns for the page allocators). glibc's per-thread caches take no lock,
  while each page pool has one mutex. With `root_ptr` the difference is smaller (268 vs. 381–399 ns),
  because `root_ptr` operations already serialize on a global mutex.
- **`root_ptr` costs about 3× `unique_ptr` in time and 1.8× in memory** (with `page_allocator_by_size`:
  56.8 vs. 17.7 ns per churn, 12.7 vs. 7.1 MB for 100,000 objects). It is the only one of the three that
  reclaims cycles.

## Setup

| | |
|---|---|
| CPU | Intel Core i7-4700HQ, 4 cores / 8 threads, 2.4 GHz |
| Memory | 16 GB |
| OS | Linux 5.15 (Ubuntu 22.04) |
| Compiler | clang 23 (`-std=c++20 -O2 -DNDEBUG`), libstdc++ 12, glibc 2.35, Boost.Pool from `/usr/include` |
| Library | `/opt/fornux/superset/usr/include` (root_ptr.hpp with the 2026-09-26 allocator fixes) |
| Load | Shared desktop machine (browser, Xorg); load average 1.6 at the start, up to 3.6 during the threaded runs |

Allocators:

| name | allocator |
|---|---|
| `pool` | `boost::pool_allocator` (the default) |
| `fast` | `boost::fast_pool_allocator` |
| `std` | `std::allocator` (glibc `malloc`) |
| `page_type` | `boost::page_allocator_by_type` (pages hold one type) |
| `page_size` | `boost::page_allocator_by_size` (pages hold one size class) |

Pointer types. `run<P, A>()` takes the pointer type as a template template argument, and `make<P, A, T>()`
selects the construction with `if constexpr`; every pointer allocates through the allocator under test:

| pointer | construction | block for a 48-byte payload | handle |
|---|---|---|---|
| `boost::root_ptr` | `root_ptr<T>(x, new node<T, A<T>>(...))` | 72 B (`node<T, A>`) | 32 B |
| `std::shared_ptr` | `std::allocate_shared<T>(A<T>(), ...)` | 64 B (control block + object) | 16 B |
| `std::unique_ptr` | `allocate_unique<T>(A<T>(), ...)`, a helper whose deleter frees through the same allocator (the standard has no allocator-aware `make_unique`) | 48 B | 8 B |

Scenarios. "Medium" is a 48-byte payload.

| scenario | what one operation is |
|---|---|
| `raw_churn` | allocator only, no pointer: allocate one node-sized block and free it at once (4,000,000 times) |
| `raw_batch` | allocator only, no pointer: allocate 100,000 blocks, then free them in allocation order |
| `churn` | create a pointer to a new Medium object and drop it (1,000,000 times) |
| `bulk` | 100,000 live Medium objects, all released together at the end of the scope |
| `mixed` | 50,000 each of 16-, 48- and 200-byte payloads interleaved, released together |
| `cycles` | 25,000 two-node cycles, reclaimed with their `node_proxy`; `root_ptr` only (a `unique_ptr` cannot form a cycle, and a `shared_ptr` cycle is never freed) |
| `threads` | 4 threads, each creating and dropping 250,000 objects (in its own `node_proxy` for `root_ptr`) |
| `types` | 32 distinct types of equal size, 100 live objects of each, released together |

Method: every (pointer, allocator, scenario) triple runs in a fresh process, so memory freed by one
scenario is never reused by the next. Tables report the median of 5 runs, interleaved across allocators
so that drift in machine load affects them all alike. Time is wall clock per operation, including the release
of every object; in `threads` it is the wall clock divided by the operations of all four threads.
Memory is the growth of resident set size (RSS, from `/proc/self/statm`) while the objects are live,
including the process's first-use costs, visible as a floor of about 1.9 MB in `types`. An empty
`asm volatile` barrier marks every created object as used: without it, clang removed the whole
`new`/`delete` pair for `unique_ptr` with `std::allocator` (0.3 ns per "churn").

## Results

### Allocator alone

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 42.2 (1.00x) | 17.1 (2.47x) | 16.4 (2.58x) | 16.8 (2.51x) | 16.8 (2.51x) |
| raw_batch | 71312.4 (1.00x) | 29.2 (2443.04x) | 28.6 (2489.09x) | 25.3 (2814.22x) | 24.1 (2965.17x) |

### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 78.2 (1.00x) | 57.4 (1.36x) | 54.3 (1.44x) | 57.2 (1.37x) | 56.8 (1.38x) |
| bulk | 144082.5 (1.00x) | 113.4 (1270.57x) | 120.3 (1197.99x) | 108.3 (1329.79x) | 108.6 (1326.24x) |
| mixed | 98512.7 (1.00x) | 148.9 (661.69x) | 155.0 (635.57x) | 137.5 (716.25x) | 133.5 (737.81x) |
| cycles | 68608.3 (1.00x) | 128.9 (532.30x) | 138.7 (494.76x) | 120.6 (568.75x) | 119.6 (573.79x) |
| threads | 448.2 (1.00x) | 391.7 (1.14x) | 268.1 (1.67x) | 399.4 (1.12x) | 381.1 (1.18x) |
| types | 231.2 (1.00x) | 142.8 (1.62x) | 136.2 (1.70x) | 338.0 (0.68x) | 126.4 (1.83x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13980 | 14072 | 12732 | 12700 | 12740 |
| mixed | 27956 | 28048 | 24372 | 23820 | 23804 |
| types | 2148 | 2244 | 2332 | 3936 | 2288 |

### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 47.4 (1.00x) | 25.5 (1.86x) | 15.1 (3.14x) | 17.2 (2.75x) | 17.7 (2.68x) |
| bulk | 84393.3 (1.00x) | 52.8 (1599.57x) | 57.0 (1479.81x) | 39.2 (2153.44x) | 39.4 (2140.33x) |
| mixed | 95397.6 (1.00x) | 78.7 (1212.17x) | 103.6 (920.83x) | 61.7 (1546.40x) | 61.0 (1565.18x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 284.2 (1.00x) | 229.7 (1.24x) | 4.2 (68.33x) | 186.3 (1.53x) | 157.5 (1.80x) |
| types | 170.8 (1.00x) | 68.9 (2.48x) | 64.1 (2.67x) | 262.8 (0.65x) | 66.0 (2.59x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 8496 | 8860 | 8668 | 7136 | 7132 |
| mixed | 19856 | 19932 | 17680 | 16116 | 16220 |
| types | 1904 | 2044 | 1912 | 3744 | 1932 |

### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 65.8 (1.00x) | 45.5 (1.45x) | 18.6 (3.55x) | 20.8 (3.17x) | 20.8 (3.16x) |
| bulk | 143421.5 (1.00x) | 97.8 (1465.88x) | 69.4 (2066.30x) | 57.2 (2506.49x) | 59.1 (2425.12x) |
| mixed | 127327.6 (1.00x) | 120.4 (1057.89x) | 117.6 (1082.72x) | 90.2 (1411.30x) | 87.8 (1449.37x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 435.4 (1.00x) | 345.9 (1.26x) | 10.0 (43.41x) | 182.9 (2.38x) | 188.3 (2.31x) |
| types | 214.8 (1.00x) | 99.0 (2.17x) | 80.5 (2.67x) | 283.3 (0.76x) | 72.1 (2.98x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 11392 | 11452 | 11084 | 9544 | 9564 |
| mixed | 24064 | 24108 | 21096 | 19824 | 19924 |
| types | 1900 | 1964 | 1880 | 3812 | 1924 |

### Pointer types side by side

Time, ns per operation, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 78.2 / 47.4 / 65.8 | 57.4 / 25.5 / 45.5 | 54.3 / 15.1 / 18.6 | 57.2 / 17.2 / 20.8 | 56.8 / 17.7 / 20.8 |
| bulk | 144082.5 / 84393.3 / 143421.5 | 113.4 / 52.8 / 97.8 | 120.3 / 57.0 / 69.4 | 108.3 / 39.2 / 57.2 | 108.6 / 39.4 / 59.1 |
| mixed | 98512.7 / 95397.6 / 127327.6 | 148.9 / 78.7 / 120.4 | 155.0 / 103.6 / 117.6 | 137.5 / 61.7 / 90.2 | 133.5 / 61.0 / 87.8 |
| cycles | 68608.3 / n/a / n/a | 128.9 / n/a / n/a | 138.7 / n/a / n/a | 120.6 / n/a / n/a | 119.6 / n/a / n/a |
| threads | 448.2 / 284.2 / 435.4 | 391.7 / 229.7 / 345.9 | 268.1 / 4.2 / 10.0 | 399.4 / 186.3 / 182.9 | 381.1 / 157.5 / 188.3 |
| types | 231.2 / 170.8 / 214.8 | 142.8 / 68.9 / 99.0 | 136.2 / 64.1 / 80.5 | 338.0 / 262.8 / 283.3 | 126.4 / 66.0 / 72.1 |

Resident memory grown, kB, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13980 / 8496 / 11392 | 14072 / 8860 / 11452 | 12732 / 8668 / 11084 | 12700 / 7136 / 9544 | 12740 / 7132 / 9564 |
| mixed | 27956 / 19856 / 24064 | 28048 / 19932 / 24108 | 24372 / 17680 / 21096 | 23820 / 16116 / 19824 | 23804 / 16220 / 19924 |
| types | 2148 / 1904 / 1900 | 2244 / 2044 / 1964 | 2332 / 1912 / 1880 | 3936 / 3744 / 3812 | 2288 / 1932 / 1924 |

### Bulk release scaling

ns per object, one run per size (single runs, so an outlier such as `root_ptr` / `page_size` at 50,000 is
measurement noise):

`boost::root_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 22505.0 | 121.2 | 156.3 | 117.7 | 123.9 |
| 25000 | 41257.5 | 126.8 | 125.1 | 127.9 | 121.1 |
| 50000 | 77308.3 | 127.7 | 144.2 | 164.4 | 258.4 |
| 100000 | 163410.2 | 129.6 | 131.1 | 124.2 | 145.8 |
| 200000 | 435531.6 | 131.3 | 134.2 | 138.7 | 125.2 |

`std::unique_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 15811.5 | 62.0 | 66.8 | 42.8 | 72.6 |
| 25000 | 26923.9 | 68.8 | 59.2 | 45.5 | 46.5 |
| 50000 | 49385.3 | 52.4 | 120.2 | 41.8 | 53.0 |
| 100000 | 99038.1 | 65.6 | 102.5 | 74.1 | 46.5 |
| 200000 | 252785.1 | 67.8 | 102.0 | 51.0 | 58.9 |

`std::shared_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 22859.6 | 98.6 | 82.4 | 65.4 | 62.4 |
| 25000 | 41148.6 | 144.7 | 73.0 | 90.8 | 75.4 |
| 50000 | 78028.7 | 101.3 | 105.0 | 61.6 | 75.5 |
| 100000 | 168313.3 | 93.5 | 78.3 | 61.8 | 64.9 |
| 200000 | 428479.8 | 108.7 | 81.7 | 67.4 | 61.7 |

## Analysis

**The default pool.** `boost::pool_allocator` keeps its free list sorted by address, so each
`deallocate` is a linear search. That doesn't show when one object is allocated and freed at a time
(`raw_churn`, `churn`: the list stays short), but every scenario that releases many objects together
pays per object in proportion to the number already free. The effect is the same for all three
pointer types: in the scaling tables the default pool's cost per object goes from 16–23 µs at 12,500
objects to 253–436 µs at 200,000, while every other allocator stays flat. Releasing a `node_proxy`
scope with many nodes is exactly what transformed programs do, so this affects them directly.

**Page allocators vs. `fast_pool_allocator` and `std::allocator`.** All three avoid the sorted list and
free in constant time. For single-object churn with `unique_ptr` and `shared_ptr`, `std::allocator` is
12–17% faster than the page allocators: glibc serves it from a lock-free per-thread cache, while a page
pool always takes its mutex. The page allocators are the fastest single-threaded allocators with
`unique_ptr` and `shared_ptr` in `bulk` and `mixed` (up to 1.7× faster than `std`), and within a few
percent of the others with `root_ptr`, where the pointer's own bookkeeping dominates. They also use the
least memory: 9–18% less than `pool` and `fast`, and up to 18% less than `std` (the same as `std` for
`root_ptr` in `bulk`). The Boost pools grow in
doubling chunks, so part of their last chunk is reserved but unused; the page allocators grow one
64 KiB page at a time and have no per-block header.

**By type vs. by size.** With one to three types (`bulk`, `mixed`) the two policies behave the same,
because each type has its own size class anyway. With many types of the same size (`types`),
`by_type` opens one 64 KiB page per type. Building a page's free list writes into every block, so the
whole page becomes resident even when it holds a handful of objects: 1.6–1.9 MB more than `by_size`
and 2.6× to 4× the time. `by_type` pays off only when keeping types apart matters more than footprint,
for example to confine a use-after-free of one type to that type's pages.

**Threads.** For `unique_ptr` and `shared_ptr`, glibc's per-thread caches make `std::allocator` 18–38×
faster than any pool here. All four threads allocate the same type, so they contend on a single page
pool's mutex (or Boost's singleton-pool mutex). For `root_ptr` the gap shrinks to 1.4×, because
`root_ptr` itself takes a global recursive mutex on every operation.

**Pointer types.** With `page_allocator_by_size`, `root_ptr` costs 3.2× `unique_ptr` and 2.7× `shared_ptr`
per churn, and 2.8× / 1.8× in bulk. The difference is the per-object state: a 72-byte node and a 32-byte
handle (the handle's links in its `node_proxy` root set), against 48 + 8 bytes for `unique_ptr` and
64 + 16 for `shared_ptr`. That matches the measured memory: 12.7 MB vs. 7.1 MB and 9.6 MB for 100,000
objects. What it buys is the `cycles` row: `root_ptr` reclaims cycles at about 120 ns per node, which
neither standard pointer can do.

## Recommendations

1. **Change the default node allocator.** `page_allocator_by_size` or `fast_pool_allocator` removes the
   quadratic release at no cost in any single-threaded scenario measured here, and `by_size` also
   saves memory. This is a library default, so it's the maintainer's decision;
   `-D BOOST_ROOT_PTR_ALLOCATOR=...` already selects an allocator per build.
2. **Carve pages lazily.** `page_pool::grow()` threads the free list through a whole page up front,
   touching all of it. Handing out blocks from a bump pointer and only threading freed blocks would
   make a sparsely used page cost only what it holds, which is what hurts `page_allocator_by_type`.
3. **Add per-thread caches to the page allocators**, in front of the shared pools. That is what makes
   `std::allocator` win under contention; for `root_ptr` the global mutex limits the benefit.

## Reproducing

```sh
bench/run.sh 5            # measure everything and print the tables (about 15 minutes)
bench/run.sh --tables     # reprint the tables from bench/raw-main.txt and bench/raw-scale.txt
# a single measurement:
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    bench/allocbench.cpp -o allocbench -lboost_thread -lpthread
./allocbench shared page_size bulk     # <pointer> <allocator> <scenario> [n]
```

The raw measurements behind these tables are in `bench/raw-main.txt` and `bench/raw-scale.txt`, and the
complete output of the run, with start and end load averages, is in `bench/bench-run.log`.
