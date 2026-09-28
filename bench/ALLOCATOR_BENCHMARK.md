# Node allocator benchmark: page allocators vs. the default pool

> **Update 2026-09-27:** following this benchmark, the default node allocator is now
> `boost::page_allocator_by_size` (`BOOST_ROOT_PTR_ALLOCATOR` in `detail/node_base.hpp`). In the
> tables, `pool` is `boost::pool_allocator`, the default when the benchmark was first run;
> `-D BOOST_ROOT_PTR_ALLOCATOR=boost::pool_allocator` restores it.

Measured 2026-09-27 with `bench/allocbench.cpp` and `bench/run.sh`, for three smart pointer types
(`boost::root_ptr`, `std::unique_ptr`, `std::shared_ptr`) and five builds: with thread support (the
default), with `BOOST_DISABLE_THREADS`, with `BOOST_ROOT_PTR_STRIPED_LOCKS`, with
`BOOST_PAGE_ALLOCATOR_THREAD_CACHE`, and with both of the last two. The same results as charts:
`bench/ALLOCATOR_BENCHMARK.pdf` (drawn by `bench/plot.py`).

## Summary

- **`boost::pool_allocator` is quadratic when many objects are released.** Its `deallocate` calls
  `ordered_free`, which walks the free list to keep it sorted (`simple_segregated_storage::find_prev`
  in Boost.Pool). This does not depend on the pointer type or the build: releasing 200,000 objects at
  once costs 229–438 µs per object, and the cost per object grows ×1.7 to ×2.6 each time the count
  doubles. Every other allocator stays flat, at 39–171 ns per object (single runs).
- **The page allocators are the fastest in every bulk, mixed-size and cycle release, in both
  single-threaded builds** (`by_size` within −4% to +7% of `by_type`). Compared with `pool` they are
  570× to 2,900× faster there, and `page_allocator_by_size` holds the same objects in 9–19% less memory.
  Churn (one object created and dropped at a time) is the exception:
  - with thread support, `std::allocator` is 5–16% faster (glibc's per-thread cache takes no lock,
    while a page pool takes its mutex). The per-thread caches reverse this (next point);
  - without thread support, `fast_pool_allocator` is 1.8–2.1× faster for `unique_ptr` and `shared_ptr`;
    for `root_ptr` the page allocators stay fastest.
- **Per-thread caches (`BOOST_PAGE_ALLOCATOR_THREAD_CACHE`) remove the page pool's mutex from the
  common path.**
  - With four threads, `page_allocator_by_size` goes from 157 to 1.9 ns per `unique_ptr` and from
    149 to 2.4 ns per `shared_ptr`, 2.2× and 2.0× faster than `std::allocator` (4.1 and 4.8 ns).
  - Combined with striped locks, `root_ptr` goes from 289 to 27 ns, against 30 ns with
    `std::allocator`.
  - Churn is 2.3–3.1× faster for the allocator alone and the standard pointers, and 1.2–1.3× for
    `root_ptr`. With the caches, churn with thread support costs what it costs without thread support
    (6.1 vs. 6.2 ns per `unique_ptr`).
  - Bulk, mixed, cycle and type scenarios and resident memory do not change beyond run-to-run noise.
  - The option is off by default for now.
- **Thread support is the largest single cost for `root_ptr`.** Built with `BOOST_DISABLE_THREADS`,
  `root_ptr` churn drops from 56.2 to 9.4 ns (6.0×), bulk release from 109.0 to 63.9 ns and cycles from
  119.6 to 45.8 ns per node, with `page_allocator_by_size`. The saving is the global `root_ptr` mutex,
  atomic reference counts and the page pool's mutex. `std::allocator` alone and with the standard
  pointers stays within run-to-run noise (0.92–1.06×): glibc and libstdc++ ignore the macro.
- **Striped locks (`BOOST_ROOT_PTR_STRIPED_LOCKS`) trade single-threaded speed for scaling.** With
  four threads and `std::allocator`, `root_ptr` goes from 271 to 30 ns per object (9.0×). With the
  pool allocators the gain is only 1.3–1.4×, because their own mutex then serializes the threads; the
  per-thread caches lift the page allocators to 10.7× (27 ns). Single-threaded, every scenario is
  1.33× to 3.35× slower: about 8 lock operations per object instead of 3. The option stays off by
  default.
- **`root_ptr` against the standard pointers:** with thread support it costs 3.4× `unique_ptr` and
  2.9× `shared_ptr` per churn. Without thread support it costs 1.5× `unique_ptr` and 1.03× `shared_ptr`,
  in 1.8× the memory of `unique_ptr`. It is the only one of the three that reclaims cycles.
- **`page_allocator_by_type` costs more with many sparse types:** 32 types of 100 objects each take
  2.4× to 4.9× the time of `by_size` and 1.6–1.8 MB more memory. Each type fills a 64 KiB page of its
  own, and a new page is fully touched when its free list is built.
- **Without the caches, `std::allocator` is far faster with four threads for `unique_ptr` and
  `shared_ptr`**: 4–5 ns per object, 31–38× faster than `page_allocator_by_size` (149–157 ns). With
  `root_ptr` and its global lock the gap is 1.4× (271 vs. 387 ns).

## Setup

| | |
|---|---|
| CPU | Intel Core i7-4700HQ, 4 cores / 8 threads, 2.4 GHz |
| Memory | 16 GB |
| OS | Linux 5.15 (Ubuntu 22.04) |
| Compiler | clang 23 (`-std=c++20 -O2 -DNDEBUG`), libstdc++ 12, glibc 2.35, Boost.Pool from `/usr/include` |
| Library | `/opt/fornux/superset/usr/include` (root_ptr.hpp, node_base.hpp and page_allocator.hpp as of 2026-09-27) |
| Load | Shared desktop machine (browser, Xorg); load average 2.0 at the start of the run (the run took 40 minutes) |

Builds. `run.sh` compiles `allocbench.cpp` five times:

| build | defines | what changes |
|---|---|---|
| with thread support (default) | none | `root_ptr` locks a global recursive mutex on every operation; node reference counts are atomic; Boost pools and page pools lock a mutex |
| without thread support | `BOOST_DISABLE_THREADS` | no `root_ptr` lock; plain reference counts (`sp_counted_base_nt`); Boost pools use `null_mutex`; page pools use a no-op mutex. `std::allocator` (glibc) and `std::shared_ptr`'s counts (libstdc++) do not change. Single-threaded scenarios only |
| striped locks | `BOOST_ROOT_PTR_STRIPED_LOCKS` | `root_ptr`'s global mutex is replaced by 1024 spin locks indexed by root address: a root's ring links and pointers are guarded by its stripe, a ring change takes the stripes of the roots involved and of their neighbours, and a node is released only after every stripe is unlocked. Measured for `root_ptr` only, which is the only pointer that uses them |
| per-thread caches | `BOOST_PAGE_ALLOCATOR_THREAD_CACHE` | every thread that allocates from a page pool keeps up to 16 KiB of its free blocks (8 to 256 blocks), and moves half of that to or from the pool under its mutex at once; a thread that exits returns its blocks. Only the page allocators change: the other allocators are the control |
| striped locks and per-thread caches | both | both of the above; measured for `root_ptr` only |

Allocators:

| name | allocator |
|---|---|
| `pool` | `boost::pool_allocator` (the default when first measured) |
| `fast` | `boost::fast_pool_allocator` |
| `std` | `std::allocator` (glibc `malloc`) |
| `page_type` | `boost::page_allocator_by_type` (pages hold one type) |
| `page_size` | `boost::page_allocator_by_size` (pages hold one size class; the default since 2026-09-27) |

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
| `threads` | 4 threads, each creating and dropping 250,000 objects (in its own `node_proxy` for `root_ptr`); not in the build without thread support |
| `types` | 32 distinct types of equal size, 100 live objects of each, released together |

Method: every (build, pointer, allocator, scenario) combination runs in a fresh process, so memory
freed by one scenario is never reused by the next. Tables report the median of 5 runs, interleaved
across allocators so that drift in machine load affects them all alike. Time is wall clock per
operation, including the release of every object; in `threads` it is the wall clock divided by the
operations of all four threads. Memory is the growth of resident set size (RSS, from
`/proc/self/statm`) while the objects are live, including the process's first-use costs, visible as a
floor of about 1.9 MB in `types`. An empty `asm volatile` barrier marks every created object as used:
without it, clang removed the whole `new`/`delete` pair for `unique_ptr` with `std::allocator`.

## Results

### Built with thread support (the default)

#### Allocator alone

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 40.6 (1.00x) | 17.5 (2.32x) | 15.5 (2.63x) | 16.6 (2.44x) | 16.8 (2.42x) |
| raw_batch | 69462.3 (1.00x) | 25.6 (2710.20x) | 27.6 (2513.11x) | 24.2 (2864.42x) | 25.4 (2734.74x) |

#### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 77.9 (1.00x) | 57.6 (1.35x) | 53.3 (1.46x) | 57.4 (1.36x) | 56.2 (1.38x) |
| bulk | 138882.6 (1.00x) | 117.0 (1187.54x) | 115.1 (1206.42x) | 107.5 (1292.29x) | 109.0 (1274.62x) |
| mixed | 92700.9 (1.00x) | 136.8 (677.59x) | 151.9 (610.32x) | 127.6 (726.33x) | 128.3 (722.70x) |
| cycles | 67957.0 (1.00x) | 123.3 (551.20x) | 133.7 (508.17x) | 119.1 (570.68x) | 119.6 (568.30x) |
| threads | 441.6 (1.00x) | 368.2 (1.20x) | 271.4 (1.63x) | 386.2 (1.14x) | 386.9 (1.14x) |
| types | 238.2 (1.00x) | 131.6 (1.81x) | 124.6 (1.91x) | 322.4 (0.74x) | 131.7 (1.81x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13960 | 14032 | 12728 | 12740 | 12708 |
| mixed | 28000 | 28040 | 24396 | 23804 | 23820 |
| types | 2108 | 2248 | 2288 | 3856 | 2280 |

#### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.2 (1.00x) | 25.4 (1.82x) | 15.4 (3.00x) | 17.2 (2.69x) | 16.4 (2.81x) |
| bulk | 83229.7 (1.00x) | 51.4 (1619.57x) | 53.6 (1553.95x) | 39.1 (2128.09x) | 39.5 (2107.08x) |
| mixed | 79149.8 (1.00x) | 76.0 (1040.76x) | 93.4 (847.61x) | 62.0 (1276.20x) | 64.0 (1237.30x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 259.8 (1.00x) | 220.8 (1.18x) | 4.1 (62.76x) | 165.8 (1.57x) | 156.9 (1.66x) |
| types | 176.8 (1.00x) | 69.1 (2.56x) | 61.3 (2.89x) | 254.5 (0.69x) | 52.4 (3.37x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 8476 | 8520 | 8748 | 7124 | 7208 |
| mixed | 19848 | 19924 | 17688 | 16152 | 16172 |
| types | 1872 | 1976 | 1908 | 3768 | 2004 |

#### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 62.9 (1.00x) | 43.1 (1.46x) | 17.1 (3.69x) | 19.8 (3.18x) | 19.4 (3.25x) |
| bulk | 134578.3 (1.00x) | 79.0 (1703.95x) | 66.3 (2028.61x) | 51.5 (2610.64x) | 53.4 (2521.14x) |
| mixed | 92856.5 (1.00x) | 108.0 (860.18x) | 104.1 (892.34x) | 77.1 (1204.36x) | 81.6 (1137.81x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 359.3 (1.00x) | 290.8 (1.24x) | 4.8 (75.63x) | 150.2 (2.39x) | 149.0 (2.41x) |
| types | 206.0 (1.00x) | 104.1 (1.98x) | 72.8 (2.83x) | 271.6 (0.76x) | 73.8 (2.79x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 11408 | 11484 | 11104 | 9576 | 9532 |
| mixed | 24076 | 24124 | 21092 | 19812 | 19924 |
| types | 1884 | 1940 | 1904 | 3716 | 1932 |

#### Pointer types side by side

Time, ns per operation, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 77.9 / 46.2 / 62.9 | 57.6 / 25.4 / 43.1 | 53.3 / 15.4 / 17.1 | 57.4 / 17.2 / 19.8 | 56.2 / 16.4 / 19.4 |
| bulk | 138882.6 / 83229.7 / 134578.3 | 117.0 / 51.4 / 79.0 | 115.1 / 53.6 / 66.3 | 107.5 / 39.1 / 51.5 | 109.0 / 39.5 / 53.4 |
| mixed | 92700.9 / 79149.8 / 92856.5 | 136.8 / 76.0 / 108.0 | 151.9 / 93.4 / 104.1 | 127.6 / 62.0 / 77.1 | 128.3 / 64.0 / 81.6 |
| cycles | 67957.0 / n/a / n/a | 123.3 / n/a / n/a | 133.7 / n/a / n/a | 119.1 / n/a / n/a | 119.6 / n/a / n/a |
| threads | 441.6 / 259.8 / 359.3 | 368.2 / 220.8 / 290.8 | 271.4 / 4.1 / 4.8 | 386.2 / 165.8 / 150.2 | 386.9 / 156.9 / 149.0 |
| types | 238.2 / 176.8 / 206.0 | 131.6 / 69.1 / 104.1 | 124.6 / 61.3 / 72.8 | 322.4 / 254.5 / 271.6 | 131.7 / 52.4 / 73.8 |

Resident memory grown, kB, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13960 / 8476 / 11408 | 14032 / 8520 / 11484 | 12728 / 8748 / 11104 | 12740 / 7124 / 9576 | 12708 / 7208 / 9532 |
| mixed | 28000 / 19848 / 24076 | 28040 / 19924 / 24124 | 24396 / 17688 / 21092 | 23804 / 16152 / 19812 | 23820 / 16172 / 19924 |
| types | 2108 / 1872 / 1884 | 2248 / 1976 / 1940 | 2288 / 1908 / 1904 | 3856 / 3768 / 3716 | 2280 / 2004 / 1932 |

#### Bulk release scaling

ns per object, one run per size:

`boost::root_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 22613.3 | 118.5 | 117.0 | 145.0 | 125.9 |
| 25000 | 39861.7 | 116.9 | 149.1 | 126.1 | 170.7 |
| 50000 | 75243.7 | 120.7 | 136.2 | 115.6 | 115.8 |
| 100000 | 168480.2 | 155.3 | 124.7 | 117.7 | 151.1 |
| 200000 | 437673.0 | 123.7 | 137.5 | 125.6 | 127.1 |

`std::unique_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 15495.9 | 64.2 | 80.3 | 41.0 | 42.6 |
| 25000 | 26393.9 | 84.7 | 56.7 | 43.5 | 50.6 |
| 50000 | 48698.2 | 58.8 | 54.2 | 44.1 | 55.1 |
| 100000 | 93448.2 | 55.9 | 53.6 | 52.5 | 38.6 |
| 200000 | 229368.6 | 56.6 | 59.6 | 44.0 | 63.7 |

`std::shared_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 22017.8 | 85.3 | 63.8 | 63.3 | 58.5 |
| 25000 | 39126.1 | 90.2 | 81.2 | 55.0 | 93.8 |
| 50000 | 75103.4 | 96.8 | 72.2 | 69.3 | 76.0 |
| 100000 | 155416.5 | 91.2 | 68.8 | 52.3 | 63.3 |
| 200000 | 377687.2 | 86.5 | 82.9 | 85.9 | 56.9 |

### Built with `BOOST_DISABLE_THREADS`

No `root_ptr` mutex, plain reference counts, no pool mutexes; single-threaded scenarios only.

#### Allocator alone

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 27.1 (1.00x) | 3.3 (8.27x) | 15.6 (1.74x) | 4.5 (6.04x) | 4.0 (6.71x) |
| raw_batch | 70094.4 (1.00x) | 24.0 (2918.17x) | 27.9 (2514.15x) | 21.5 (3260.20x) | 20.8 (3368.30x) |

#### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 35.6 (1.00x) | 10.6 (3.37x) | 21.2 (1.68x) | 9.3 (3.83x) | 9.4 (3.80x) |
| bulk | 140134.5 (1.00x) | 70.4 (1990.26x) | 76.8 (1824.91x) | 63.8 (2198.19x) | 63.9 (2194.40x) |
| mixed | 95495.5 (1.00x) | 96.5 (989.28x) | 119.3 (800.47x) | 84.9 (1124.40x) | 85.8 (1112.87x) |
| cycles | 68285.9 (1.00x) | 56.8 (1201.58x) | 64.2 (1062.98x) | 43.5 (1569.43x) | 45.8 (1491.94x) |
| types | 185.2 (1.00x) | 91.4 (2.03x) | 81.7 (2.27x) | 274.1 (0.68x) | 74.4 (2.49x) |

#### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 27.1 (1.00x) | 3.2 (8.39x) | 15.5 (1.75x) | 5.9 (4.62x) | 6.2 (4.40x) |
| bulk | 84194.5 (1.00x) | 38.0 (2215.64x) | 54.2 (1552.26x) | 34.3 (2457.52x) | 34.0 (2477.04x) |
| mixed | 78613.1 (1.00x) | 60.8 (1294.04x) | 88.3 (889.89x) | 56.6 (1389.66x) | 55.4 (1418.50x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 152.6 (1.00x) | 44.0 (3.47x) | 63.8 (2.39x) | 242.1 (0.63x) | 50.4 (3.03x) |

#### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 30.0 (1.00x) | 4.7 (6.40x) | 18.5 (1.62x) | 9.9 (3.01x) | 9.1 (3.29x) |
| bulk | 134868.5 (1.00x) | 49.2 (2742.90x) | 64.3 (2098.14x) | 48.2 (2800.43x) | 46.3 (2914.18x) |
| mixed | 93995.9 (1.00x) | 75.7 (1241.20x) | 102.2 (919.91x) | 70.6 (1331.58x) | 75.2 (1250.28x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 163.4 (1.00x) | 62.6 (2.61x) | 75.2 (2.17x) | 277.4 (0.59x) | 68.3 (2.39x) |

### Striped locks (`BOOST_ROOT_PTR_STRIPED_LOCKS`)

`boost::root_ptr` only. Time, ns per operation, global lock / striped locks (ratio: above 1.00x the striped build is faster):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 77.9 / 132.6 (0.59x) | 57.6 / 111.8 (0.52x) | 53.3 / 113.4 (0.47x) | 57.4 / 114.6 (0.50x) | 56.2 / 113.8 (0.49x) |
| bulk | 138882.6 / 140865.4 (0.99x) | 117.0 / 206.6 (0.57x) | 115.1 / 203.9 (0.56x) | 107.5 / 198.1 (0.54x) | 109.0 / 195.5 (0.56x) |
| mixed | 92700.9 / 99231.8 (0.93x) | 136.8 / 235.6 (0.58x) | 151.9 / 232.3 (0.65x) | 127.6 / 219.1 (0.58x) | 128.3 / 219.9 (0.58x) |
| cycles | 67957.0 / 70474.8 (0.96x) | 123.3 / 394.1 (0.31x) | 133.7 / 397.0 (0.34x) | 119.1 / 380.9 (0.31x) | 119.6 / 400.7 (0.30x) |
| threads | 441.6 / 323.9 (1.36x) | 368.2 / 279.8 (1.32x) | 271.4 / 30.2 (8.99x) | 386.2 / 280.2 (1.38x) | 386.9 / 288.6 (1.34x) |
| types | 238.2 / 316.7 (0.75x) | 131.6 / 242.3 (0.54x) | 124.6 / 241.4 (0.52x) | 322.4 / 427.3 (0.75x) | 131.7 / 233.9 (0.56x) |

### Per-thread caches (`BOOST_PAGE_ALLOCATOR_THREAD_CACHE`)

Only the page allocators have them; the other columns are the control. Time, ns per operation, without / with the caches (ratio: above 1.00x the caches are faster):

`allocator alone`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 40.6 / 41.0 (0.99x) | 17.5 / 16.0 (1.10x) | 15.5 / 15.4 (1.00x) | 16.6 / 5.3 (3.12x) | 16.8 / 5.7 (2.96x) |
| raw_batch | 69462.3 / 72982.8 (0.95x) | 25.6 / 25.5 (1.01x) | 27.6 / 29.4 (0.94x) | 24.2 / 23.7 (1.02x) | 25.4 / 23.5 (1.08x) |

`boost::root_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 77.9 / 78.7 (0.99x) | 57.6 / 57.9 (1.00x) | 53.3 / 53.6 (0.99x) | 57.4 / 45.1 (1.27x) | 56.2 / 45.8 (1.23x) |
| bulk | 138882.6 / 143854.3 (0.97x) | 117.0 / 111.4 (1.05x) | 115.1 / 118.3 (0.97x) | 107.5 / 109.8 (0.98x) | 109.0 / 110.4 (0.99x) |
| mixed | 92700.9 / 95019.7 (0.98x) | 136.8 / 137.4 (1.00x) | 151.9 / 161.9 (0.94x) | 127.6 / 134.7 (0.95x) | 128.3 / 139.1 (0.92x) |
| cycles | 67957.0 / 68082.6 (1.00x) | 123.3 / 125.2 (0.99x) | 133.7 / 133.4 (1.00x) | 119.1 / 117.8 (1.01x) | 119.6 / 116.0 (1.03x) |
| threads | 441.6 / 444.4 (0.99x) | 368.2 / 381.9 (0.96x) | 271.4 / 283.2 (0.96x) | 386.2 / 252.6 (1.53x) | 386.9 / 263.2 (1.47x) |
| types | 238.2 / 227.8 (1.05x) | 131.6 / 136.9 (0.96x) | 124.6 / 127.2 (0.98x) | 322.4 / 325.9 (0.99x) | 131.7 / 126.0 (1.05x) |

`std::unique_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.2 / 45.7 (1.01x) | 25.4 / 24.6 (1.03x) | 15.4 / 15.3 (1.01x) | 17.2 / 6.2 (2.77x) | 16.4 / 6.1 (2.69x) |
| bulk | 83229.7 / 83587.9 (1.00x) | 51.4 / 51.6 (1.00x) | 53.6 / 54.0 (0.99x) | 39.1 / 36.4 (1.08x) | 39.5 / 38.5 (1.03x) |
| mixed | 79149.8 / 82575.2 (0.96x) | 76.0 / 73.4 (1.04x) | 93.4 / 90.8 (1.03x) | 62.0 / 64.5 (0.96x) | 64.0 / 63.7 (1.00x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 259.8 / 270.5 (0.96x) | 220.8 / 220.4 (1.00x) | 4.1 / 4.2 (0.98x) | 165.8 / 1.9 (86.83x) | 156.9 / 1.9 (84.33x) |
| types | 176.8 / 160.1 (1.10x) | 69.1 / 61.0 (1.13x) | 61.3 / 66.4 (0.92x) | 254.5 / 258.4 (0.98x) | 52.4 / 50.7 (1.03x) |

`std::shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 62.9 / 62.8 (1.00x) | 43.1 / 44.4 (0.97x) | 17.1 / 17.2 (0.99x) | 19.8 / 8.4 (2.35x) | 19.4 / 8.5 (2.27x) |
| bulk | 134578.3 / 135185.5 (1.00x) | 79.0 / 82.3 (0.96x) | 66.3 / 66.7 (1.00x) | 51.5 / 52.7 (0.98x) | 53.4 / 51.8 (1.03x) |
| mixed | 92856.5 / 91463.9 (1.02x) | 108.0 / 106.6 (1.01x) | 104.1 / 102.0 (1.02x) | 77.1 / 78.3 (0.98x) | 81.6 / 77.8 (1.05x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 359.3 / 376.6 (0.95x) | 290.8 / 289.7 (1.00x) | 4.8 / 4.7 (1.01x) | 150.2 / 2.5 (60.30x) | 149.0 / 2.4 (61.55x) |
| types | 206.0 / 198.5 (1.04x) | 104.1 / 98.1 (1.06x) | 72.8 / 75.2 (0.97x) | 271.6 / 263.9 (1.03x) | 73.8 / 63.2 (1.17x) |

`boost::root_ptr` with striped locks, without / with the caches:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 132.6 / 129.6 (1.02x) | 111.8 / 112.2 (1.00x) | 113.4 / 112.1 (1.01x) | 114.6 / 103.9 (1.10x) | 113.8 / 103.5 (1.10x) |
| bulk | 140865.4 / 140195.0 (1.00x) | 206.6 / 196.1 (1.05x) | 203.9 / 204.0 (1.00x) | 198.1 / 193.8 (1.02x) | 195.5 / 197.1 (0.99x) |
| mixed | 99231.8 / 95777.3 (1.04x) | 235.6 / 221.9 (1.06x) | 232.3 / 228.4 (1.02x) | 219.1 / 232.5 (0.94x) | 219.9 / 240.1 (0.92x) |
| cycles | 70474.8 / 68086.6 (1.04x) | 394.1 / 388.5 (1.01x) | 397.0 / 392.7 (1.01x) | 380.9 / 398.3 (0.96x) | 400.7 / 392.5 (1.02x) |
| threads | 323.9 / 306.6 (1.06x) | 279.8 / 283.7 (0.99x) | 30.2 / 29.3 (1.03x) | 280.2 / 28.5 (9.85x) | 288.6 / 27.0 (10.68x) |
| types | 316.7 / 332.4 (0.95x) | 242.3 / 235.6 (1.03x) | 241.4 / 228.8 (1.05x) | 427.3 / 414.1 (1.03x) | 233.9 / 225.0 (1.04x) |

Resident memory grown, kB, without / with the caches:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| root bulk | 13960 / 13976 | 14032 / 14040 | 12728 / 12744 | 12740 / 12740 | 12708 / 12712 |
| root mixed | 28000 / 27992 | 28040 / 28036 | 24396 / 24392 | 23804 / 23808 | 23820 / 23828 |
| root types | 2108 / 2148 | 2248 / 2252 | 2288 / 2296 | 3856 / 3944 | 2280 / 2264 |
| unique bulk | 8476 / 8440 | 8520 / 8564 | 8748 / 8716 | 7124 / 7184 | 7208 / 7216 |
| unique mixed | 19848 / 19808 | 19924 / 19892 | 17688 / 17644 | 16152 / 16172 | 16172 / 16236 |
| unique types | 1872 / 1900 | 1976 / 1972 | 1908 / 1916 | 3768 / 3764 | 2004 / 1984 |
| shared bulk | 11408 / 11392 | 11484 / 11468 | 11104 / 11064 | 9576 / 9548 | 9532 / 9520 |
| shared mixed | 24076 / 24092 | 24124 / 24100 | 21092 / 21068 | 19812 / 19808 | 19924 / 19928 |
| shared types | 1884 / 1980 | 1940 / 1944 | 1904 / 1924 | 3716 / 3792 | 1932 / 1992 |

### What thread support costs

Time, ns per operation, with thread support / with `BOOST_DISABLE_THREADS` (ratio):

`allocator alone`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 40.6 / 27.1 (1.50x) | 17.5 / 3.3 (5.34x) | 15.5 / 15.6 (0.99x) | 16.6 / 4.5 (3.71x) | 16.8 / 4.0 (4.15x) |
| raw_batch | 69462.3 / 70094.4 (0.99x) | 25.6 / 24.0 (1.07x) | 27.6 / 27.9 (0.99x) | 24.2 / 21.5 (1.13x) | 25.4 / 20.8 (1.22x) |

`boost::root_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 77.9 / 35.6 (2.18x) | 57.6 / 10.6 (5.45x) | 53.3 / 21.2 (2.52x) | 57.4 / 9.3 (6.16x) | 56.2 / 9.4 (5.99x) |
| bulk | 138882.6 / 140134.5 (0.99x) | 117.0 / 70.4 (1.66x) | 115.1 / 76.8 (1.50x) | 107.5 / 63.8 (1.69x) | 109.0 / 63.9 (1.71x) |
| mixed | 92700.9 / 95495.5 (0.97x) | 136.8 / 96.5 (1.42x) | 151.9 / 119.3 (1.27x) | 127.6 / 84.9 (1.50x) | 128.3 / 85.8 (1.49x) |
| cycles | 67957.0 / 68285.9 (1.00x) | 123.3 / 56.8 (2.17x) | 133.7 / 64.2 (2.08x) | 119.1 / 43.5 (2.74x) | 119.6 / 45.8 (2.61x) |
| types | 238.2 / 185.2 (1.29x) | 131.6 / 91.4 (1.44x) | 124.6 / 81.7 (1.53x) | 322.4 / 274.1 (1.18x) | 131.7 / 74.4 (1.77x) |

`std::unique_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.2 / 27.1 (1.71x) | 25.4 / 3.2 (7.86x) | 15.4 / 15.5 (1.00x) | 17.2 / 5.9 (2.93x) | 16.4 / 6.2 (2.67x) |
| bulk | 83229.7 / 84194.5 (0.99x) | 51.4 / 38.0 (1.35x) | 53.6 / 54.2 (0.99x) | 39.1 / 34.3 (1.14x) | 39.5 / 34.0 (1.16x) |
| mixed | 79149.8 / 78613.1 (1.01x) | 76.0 / 60.8 (1.25x) | 93.4 / 88.3 (1.06x) | 62.0 / 56.6 (1.10x) | 64.0 / 55.4 (1.15x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 176.8 / 152.6 (1.16x) | 69.1 / 44.0 (1.57x) | 61.3 / 63.8 (0.96x) | 254.5 / 242.1 (1.05x) | 52.4 / 50.4 (1.04x) |

`std::shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 62.9 / 30.0 (2.10x) | 43.1 / 4.7 (9.22x) | 17.1 / 18.5 (0.92x) | 19.8 / 9.9 (1.99x) | 19.4 / 9.1 (2.12x) |
| bulk | 134578.3 / 134868.5 (1.00x) | 79.0 / 49.2 (1.61x) | 66.3 / 64.3 (1.03x) | 51.5 / 48.2 (1.07x) | 53.4 / 46.3 (1.15x) |
| mixed | 92856.5 / 93995.9 (0.99x) | 108.0 / 75.7 (1.43x) | 104.1 / 102.2 (1.02x) | 77.1 / 70.6 (1.09x) | 81.6 / 75.2 (1.09x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 206.0 / 163.4 (1.26x) | 104.1 / 62.6 (1.66x) | 72.8 / 75.2 (0.97x) | 271.6 / 277.4 (0.98x) | 73.8 / 68.3 (1.08x) |

## Analysis

**`boost::pool_allocator`.** It keeps its free list sorted by address, so each `deallocate` is a
linear search. That doesn't show when one object is allocated and freed at a time (`raw_churn`,
`churn`: the list stays short), but every scenario that releases many objects together pays per
object in proportion to the number already free. The effect is the same for all three pointer types
and in every build: in the scaling tables the cost per object goes from 15–23 µs at 12,500 objects to
229–438 µs at 200,000, while every other allocator stays flat. Releasing a `node_proxy` scope with
many nodes is exactly what transformed programs do, which is why the default was changed.

**Page allocators vs. `fast_pool_allocator` and `std::allocator`.** All three avoid the sorted list
and free in constant time. In the release scenarios a page allocator is the fastest in both
single-threaded builds. They also use the least memory: 9–19% less than `pool` and `fast`, and up to 18%
less than `std` (the same as `std` for `root_ptr` in `bulk`). The Boost pools grow in doubling chunks,
so part of their last chunk is reserved but unused; the page allocators grow one 64 KiB page at a time
and have no per-block header. For churn, `std::allocator` wins with thread support, because glibc
serves it from a per-thread cache while a page pool takes its mutex (unless its per-thread caches
are on: see below). Without thread support,
`fast_pool_allocator` wins churn and "many types" for the standard pointers (3.2 vs. 6.2 ns per
`unique_ptr` churn); I haven't profiled where the page pool's extra time goes.

**By type vs. by size.** With one to three types (`bulk`, `mixed`) the two policies behave the same,
because each type has its own size class anyway. With many types of the same size (`types`),
`by_type` opens one 64 KiB page per type. Building a page's free list writes into every block, so the
whole page becomes resident even when it holds a handful of objects: 1.6–1.8 MB more than `by_size`
and 2.4× to 4.9× the time. `by_type` pays off only when keeping types apart matters more than
footprint, for example to confine a use-after-free of one type to that type's pages.

**Thread support.** With thread support, every `root_ptr` operation locks a global recursive mutex,
node reference counts are atomic, and the pools lock their own mutexes. Together that is 6.0× on
`root_ptr` churn with `page_allocator_by_size` (56.2 vs. 9.4 ns), 1.5–1.7× on bulk and mixed release
and 2.6× on cycles. About 10 ns of the churn difference is the page pool's mutex (the same drop as
`unique_ptr` churn, 16.4 to 6.2 ns, and what the per-thread caches save on `root_ptr` churn, 56.2 to
45.8 ns); the rest is `root_ptr`'s own mutex and atomic counts. The allocator-only rows show the pools'
share: 4.2× on `raw_churn` for `page_size` and 5.3× for `fast`, but 1.0× for `std`.

**Striped locks.** With `BOOST_ROOT_PTR_STRIPED_LOCKS`, `root_ptr` no longer serializes every
operation of every thread on one mutex. The price is more lock operations: a construction, access
and destruction takes about 8 instead of 3, so every single-threaded scenario is 1.33× to 3.35× slower
(except with `pool`, whose quadratic release dominates everything). With four threads the gain depends
on the allocator: 9.0× with `std::allocator`, whose per-thread caches take no lock, but only 1.3–1.4×
with the pool allocators, whose own mutex becomes the bottleneck, and 10.7× with the page allocators
once they have per-thread caches too (27 ns). Striped `root_ptr` (27–30 ns) is still 6–14× slower
than `unique_ptr` and `shared_ptr` (1.9–4.8 ns) in that scenario. Packing the locks into fewer cache lines did not help single-threaded code
and made four threads 2× slower through false sharing.

**Per-thread caches.** With `BOOST_PAGE_ALLOCATOR_THREAD_CACHE`, a thread allocates from and frees
into a stack of its own, and takes the pool's mutex only once per batch (half of 16 KiB worth of
blocks). This is what glibc does for `std::allocator`, and it closes the same gaps: four threads
allocating the same type no longer contend (157 → 1.9 ns per `unique_ptr`, 149 → 2.4 ns per
`shared_ptr`, faster than `std::allocator`'s 4.1 and 4.8 ns), and churn with thread support costs what
it costs without (6.1 vs. 6.2 ns per `unique_ptr`). With the global `root_ptr` lock, four threads gain
only 1.5×, because `root_ptr` itself then serializes them; with striped locks, 10.7×. Bulk, mixed,
cycles and types move by 0.92–1.17×, the same spread as the allocators that have no cache
(0.92–1.13×), and resident memory by −28 to +88 kB. The cost is idle memory: up to 16 KiB per thread
and per pool it allocated from, returned when the thread exits. A thread that only frees (a consumer)
never caches, and returns every block to the pool directly.

**Threads.** For `unique_ptr` and `shared_ptr`, glibc's per-thread caches make `std::allocator` 31–38×
faster than `page_allocator_by_size` without its own caches: all four threads allocate the same type, so they contend
on a single page pool's mutex (or Boost's singleton-pool mutex). For `root_ptr` with its default
global lock the gap shrinks to 1.4×, because `root_ptr` itself serializes every operation.

**Pointer types.** With `page_allocator_by_size` and thread support, `root_ptr` costs 3.4× `unique_ptr`
and 2.9× `shared_ptr` per churn, and 2.8× / 2.0× in bulk. Without thread support the gap narrows to
1.5× `unique_ptr` and 1.03× `shared_ptr` per churn, and 1.9× / 1.4× in bulk: most of `root_ptr`'s extra
cost is locking. The per-object state explains the rest: a 72-byte node and a 32-byte handle (the
handle's links in its `node_proxy` ring), against 48 + 8 bytes for `unique_ptr` and 64 + 16 for
`shared_ptr`, which matches the measured memory (12.7 MB vs. 7.2 MB and 9.5 MB for 100,000 objects).
What it buys is the `cycles` row: `root_ptr` reclaims cycles at 44–134 ns per node, which neither
standard pointer can do.

## Recommendations

1. **Change the default node allocator. Done 2026-09-27:** the default is now
   `page_allocator_by_size`, which removes the quadratic release and also saves memory.
   `-D BOOST_ROOT_PTR_ALLOCATOR=...` still selects another allocator per build.
2. **Build single-threaded programs with `BOOST_DISABLE_THREADS`.** It makes `root_ptr` up to 6.0×
   faster here. **Done 2026-09-27:** `FCXXSS_NO_THREADS=1 fcxxss.sh ...` defines it in all three
   passes, with a precompiled header of its own. The program must not use threads at all.
3. **Carve pages lazily.** `page_pool::grow()` threads the free list through a whole page up front,
   touching all of it. Handing out blocks from a bump pointer and only threading freed blocks would
   make a sparsely used page cost only what it holds, which is what hurts `page_allocator_by_type`.
4. **Add per-thread caches to the page allocators. Implemented 2026-09-27 as an option,
   `BOOST_PAGE_ALLOCATOR_THREAD_CACHE`** (ignored without thread support). With four threads the page
   allocators go from 31–38× slower than `std::allocator` to 2.0–2.2× faster, and striped `root_ptr`
   scales 10.7× instead of 1.3×.
5. **Finer-grained locking in `root_ptr`. Implemented 2026-09-27 as an option,
   `BOOST_ROOT_PTR_STRIPED_LOCKS`.** Rings are not one per `node_proxy` (a copy moves its source into a
   ring of its own, an assignment moves the source into the target's ring), so the locks are striped
   by root address instead. It pays off only with several threads, so it stays off by default.
6. **Make the per-thread caches the default in builds with thread support.** Proposed, not done:
   they were faster or equal in every scenario measured here, and the price is up to 16 KiB of idle
   blocks per thread and per pool it allocated from.

## Reproducing

```sh
bench/run.sh 5            # measure all five builds and print the tables (about 40 minutes)
bench/run.sh --tables     # reprint the tables from bench/raw-main.txt and bench/raw-scale.txt
bench/plot.py             # draw the charts from the same files into bench/ALLOCATOR_BENCHMARK.pdf
# a single measurement:
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    bench/allocbench.cpp -o allocbench -lboost_thread -lpthread
    # add -DBOOST_DISABLE_THREADS, -DBOOST_ROOT_PTR_STRIPED_LOCKS and/or
    # -DBOOST_PAGE_ALLOCATOR_THREAD_CACHE for the other builds
./allocbench shared page_size bulk     # <pointer> <allocator> <scenario> [n]
```

The raw measurements behind these tables are in `bench/raw-main.txt` (one line per run: build,
pointer, allocator, scenario, ns, kB) and `bench/raw-scale.txt`. The complete output of the run, with
start and end load averages, is in `bench/bench-run.log`.
