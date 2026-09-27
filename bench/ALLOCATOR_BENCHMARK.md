# Node allocator benchmark: page allocators vs. the default pool

> **Update 2026-09-27:** following this benchmark, the default node allocator is now
> `boost::page_allocator_by_size` (`BOOST_ROOT_PTR_ALLOCATOR` in `detail/node_base.hpp`). In the
> tables, `pool` is `boost::pool_allocator`, the default when the benchmark was first run;
> `-D BOOST_ROOT_PTR_ALLOCATOR=boost::pool_allocator` restores it.

Measured 2026-09-27 with `bench/allocbench.cpp` and `bench/run.sh`, for three smart pointer types
(`boost::root_ptr`, `std::unique_ptr`, `std::shared_ptr`) and two builds: with thread support (the
default) and with `BOOST_DISABLE_THREADS`. The same results as charts: `bench/ALLOCATOR_BENCHMARK.pdf`
(drawn by `bench/plot.py`).

## Summary

- **`boost::pool_allocator` is quadratic when many objects are released.** Its `deallocate` calls
  `ordered_free`, which walks the free list to keep it sorted (`simple_segregated_storage::find_prev`
  in Boost.Pool). This does not depend on the pointer type or on thread support: releasing 200,000
  objects at once costs 180–348 µs per object, and the cost per object grows ×1.7 to ×2.5 each time
  the count doubles. Every other allocator stays flat, at 37–120 ns per object at every size.
- **`page_allocator_by_size` is the fastest, or within 3% of the fastest, in every single-threaded
  release scenario (bulk, mixed sizes, cycles) in both builds, and in "many types" with thread
  support.** Compared with `pool` it is 560× to 2,900× faster there, and it holds the same objects in
  9–18% less memory. The exceptions are **churn** (one object created and dropped at a time) and,
  without thread support, "many types":
  - with thread support, `std::allocator` is 8–15% faster (glibc's per-thread cache takes no lock,
    while a page pool takes its mutex);
  - without thread support, `fast_pool_allocator` is 1.75–2× faster on churn and 7–8% faster on
    "many types" for `unique_ptr` and `shared_ptr`; for `root_ptr` the page allocators stay fastest.
- **Thread support is the largest single cost for `root_ptr`.** Built with `BOOST_DISABLE_THREADS`,
  `root_ptr` churn drops from 57.8 to 9.5 ns (6.1×), bulk release from 106.5 to 65.7 ns and cycles
  from 122.1 to 43.5 ns per node, with `page_allocator_by_size`. The saving is the global
  `root_ptr` mutex, atomic reference counts and the page pool's mutex. `std::allocator` is
  unaffected, alone and with `unique_ptr` or `shared_ptr` (0.97–1.04×), as it should be: glibc and
  libstdc++ ignore the macro.
- **`root_ptr` against the standard pointers:** with thread support it costs 3.3× `unique_ptr` and
  3.0× `shared_ptr` per churn. Without thread support it costs 1.6× `unique_ptr` and the same as
  `shared_ptr` (9.5 vs. 9.5 ns), in 1.8× the memory of `unique_ptr`. It is the only one of the three
  that reclaims cycles.
- **`page_allocator_by_type` costs more with many sparse types:** 32 types of 100 objects each take
  2.7× to 4.2× the time of `by_size` and 1.6–1.9 MB more memory. Each type fills a 64 KiB page of
  its own, and a new page is fully touched when its free list is built.
- **With four threads, `std::allocator` is far faster for `unique_ptr` and `shared_ptr`**: 4–5 ns
  per object, 28–36× faster than the page allocators (148–151 ns). glibc's per-thread caches take no
  lock, while each page pool has one mutex. With `root_ptr` the gap is 1.5× (264 vs. 384–390 ns),
  because `root_ptr` operations already serialize on a global mutex.

## Setup

| | |
|---|---|
| CPU | Intel Core i7-4700HQ, 4 cores / 8 threads, 2.4 GHz |
| Memory | 16 GB |
| OS | Linux 5.15 (Ubuntu 22.04) |
| Compiler | clang 23 (`-std=c++20 -O2 -DNDEBUG`), libstdc++ 12, glibc 2.35, Boost.Pool from `/usr/include` |
| Library | `/opt/fornux/superset/usr/include` (root_ptr.hpp and page_allocator.hpp as of 2026-09-27) |
| Load | Shared desktop machine (browser, Xorg); load average about 1.4 at the start and at the end of the run |

Builds. `run.sh` compiles `allocbench.cpp` twice:

| build | defines | what changes |
|---|---|---|
| with thread support (default) | none | `root_ptr` locks a global recursive mutex on every operation; node reference counts are atomic; Boost pools and page pools lock a mutex |
| without thread support | `BOOST_DISABLE_THREADS` | no `root_ptr` lock; plain reference counts (`sp_counted_base_nt`); Boost pools use `null_mutex`; page pools use a no-op mutex. `std::allocator` (glibc) and `std::shared_ptr`'s counts (libstdc++) do not change. Single-threaded scenarios only |

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
| `threads` | 4 threads, each creating and dropping 250,000 objects (in its own `node_proxy` for `root_ptr`); thread-support build only |
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
| raw_churn | 43.2 (1.00x) | 17.5 (2.47x) | 15.8 (2.74x) | 16.8 (2.57x) | 16.9 (2.56x) |
| raw_batch | 76550.6 (1.00x) | 25.7 (2974.00x) | 28.2 (2716.49x) | 25.3 (3022.13x) | 24.5 (3121.97x) |

#### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 78.3 (1.00x) | 57.5 (1.36x) | 53.6 (1.46x) | 57.0 (1.37x) | 57.8 (1.35x) |
| bulk | 146740.8 (1.00x) | 113.5 (1292.76x) | 115.9 (1265.99x) | 105.3 (1393.29x) | 106.5 (1378.11x) |
| mixed | 100748.9 (1.00x) | 142.1 (709.25x) | 155.9 (646.07x) | 131.9 (763.94x) | 132.7 (759.51x) |
| cycles | 68706.2 (1.00x) | 129.6 (530.14x) | 136.8 (502.42x) | 119.8 (573.51x) | 122.1 (562.57x) |
| threads | 463.7 (1.00x) | 389.5 (1.19x) | 264.3 (1.75x) | 383.6 (1.21x) | 390.0 (1.19x) |
| types | 232.6 (1.00x) | 135.3 (1.72x) | 136.5 (1.70x) | 342.3 (0.68x) | 128.1 (1.82x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13984 | 14036 | 12732 | 12720 | 12748 |
| mixed | 27976 | 28028 | 24332 | 23848 | 23756 |
| types | 2172 | 2212 | 2292 | 3900 | 2288 |

#### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.9 (1.00x) | 26.1 (1.80x) | 15.3 (3.06x) | 18.0 (2.61x) | 17.6 (2.66x) |
| bulk | 84289.7 (1.00x) | 52.7 (1600.03x) | 55.6 (1516.27x) | 40.1 (2100.42x) | 40.2 (2097.28x) |
| mixed | 80728.8 (1.00x) | 74.3 (1086.67x) | 91.0 (887.32x) | 59.8 (1349.08x) | 60.2 (1341.90x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 231.1 (1.00x) | 224.4 (1.03x) | 4.2 (55.56x) | 150.5 (1.54x) | 148.0 (1.56x) |
| types | 179.6 (1.00x) | 60.7 (2.96x) | 62.3 (2.88x) | 247.2 (0.73x) | 58.9 (3.05x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 8452 | 8548 | 8720 | 7176 | 7124 |
| mixed | 19836 | 19900 | 17668 | 16088 | 16228 |
| types | 1904 | 1960 | 1936 | 3784 | 1924 |

#### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 62.1 (1.00x) | 43.8 (1.42x) | 17.7 (3.52x) | 19.8 (3.13x) | 19.6 (3.17x) |
| bulk | 138642.1 (1.00x) | 79.7 (1739.55x) | 64.4 (2154.17x) | 54.6 (2539.23x) | 51.7 (2683.74x) |
| mixed | 98903.4 (1.00x) | 108.4 (912.48x) | 111.6 (886.39x) | 77.9 (1270.11x) | 78.5 (1260.72x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 367.9 (1.00x) | 295.6 (1.24x) | 5.3 (69.94x) | 148.6 (2.48x) | 148.4 (2.48x) |
| types | 195.1 (1.00x) | 95.1 (2.05x) | 73.7 (2.65x) | 258.4 (0.75x) | 71.4 (2.73x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 11360 | 11488 | 11084 | 9568 | 9572 |
| mixed | 24096 | 24048 | 21060 | 19848 | 19920 |
| types | 1932 | 1964 | 1920 | 3720 | 1948 |

#### Pointer types side by side

Time, ns per operation, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 78.3 / 46.9 / 62.1 | 57.5 / 26.1 / 43.8 | 53.6 / 15.3 / 17.7 | 57.0 / 18.0 / 19.8 | 57.8 / 17.6 / 19.6 |
| bulk | 146740.8 / 84289.7 / 138642.1 | 113.5 / 52.7 / 79.7 | 115.9 / 55.6 / 64.4 | 105.3 / 40.1 / 54.6 | 106.5 / 40.2 / 51.7 |
| mixed | 100748.9 / 80728.8 / 98903.4 | 142.1 / 74.3 / 108.4 | 155.9 / 91.0 / 111.6 | 131.9 / 59.8 / 77.9 | 132.7 / 60.2 / 78.5 |
| cycles | 68706.2 / n/a / n/a | 129.6 / n/a / n/a | 136.8 / n/a / n/a | 119.8 / n/a / n/a | 122.1 / n/a / n/a |
| threads | 463.7 / 231.1 / 367.9 | 389.5 / 224.4 / 295.6 | 264.3 / 4.2 / 5.3 | 383.6 / 150.5 / 148.6 | 390.0 / 148.0 / 148.4 |
| types | 232.6 / 179.6 / 195.1 | 135.3 / 60.7 / 95.1 | 136.5 / 62.3 / 73.7 | 342.3 / 247.2 / 258.4 | 128.1 / 58.9 / 71.4 |

Resident memory grown, kB, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13984 / 8452 / 11360 | 14036 / 8548 / 11488 | 12732 / 8720 / 11084 | 12720 / 7176 / 9568 | 12748 / 7124 / 9572 |
| mixed | 27976 / 19836 / 24096 | 28028 / 19900 / 24048 | 24332 / 17668 / 21060 | 23848 / 16088 / 19848 | 23756 / 16228 / 19920 |
| types | 2172 / 1904 / 1932 | 2212 / 1960 / 1964 | 2292 / 1936 / 1920 | 3900 / 3784 / 3720 | 2288 / 1924 / 1948 |

#### Bulk release scaling

ns per object, one run per size:

`boost::root_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 20886.6 | 113.3 | 119.6 | 117.3 | 111.0 |
| 25000 | 37212.3 | 116.1 | 116.1 | 109.8 | 109.5 |
| 50000 | 68743.6 | 113.7 | 112.7 | 102.9 | 105.8 |
| 100000 | 140610.8 | 115.5 | 118.4 | 107.3 | 105.8 |
| 200000 | 347740.5 | 110.9 | 116.2 | 104.5 | 106.0 |

`std::unique_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 13813.9 | 52.2 | 56.1 | 42.1 | 45.9 |
| 25000 | 24315.8 | 51.6 | 56.5 | 38.7 | 38.7 |
| 50000 | 43783.5 | 51.7 | 54.2 | 37.4 | 39.5 |
| 100000 | 83708.9 | 52.8 | 54.1 | 37.9 | 38.2 |
| 200000 | 180424.4 | 54.4 | 53.8 | 40.3 | 40.6 |

`std::shared_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 21026.3 | 86.2 | 65.9 | 56.9 | 52.9 |
| 25000 | 36156.0 | 85.4 | 66.0 | 53.0 | 55.0 |
| 50000 | 68391.6 | 81.7 | 65.0 | 53.5 | 51.7 |
| 100000 | 134446.4 | 78.6 | 63.9 | 51.2 | 52.2 |
| 200000 | 306791.6 | 83.2 | 65.5 | 55.8 | 55.4 |

### Built with `BOOST_DISABLE_THREADS`

No `root_ptr` mutex, plain reference counts, no pool mutexes; single-threaded scenarios only.

#### Allocator alone

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 27.3 (1.00x) | 3.3 (8.30x) | 15.5 (1.76x) | 4.2 (6.56x) | 3.9 (6.95x) |
| raw_batch | 70657.7 (1.00x) | 23.3 (3029.92x) | 28.0 (2527.10x) | 21.3 (3323.50x) | 21.4 (3294.07x) |

#### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 36.4 (1.00x) | 11.1 (3.28x) | 20.2 (1.80x) | 9.4 (3.86x) | 9.5 (3.84x) |
| bulk | 141721.0 (1.00x) | 69.8 (2031.84x) | 79.2 (1790.31x) | 65.8 (2154.80x) | 65.7 (2156.44x) |
| mixed | 99023.1 (1.00x) | 96.8 (1023.18x) | 119.2 (830.94x) | 84.9 (1166.35x) | 84.6 (1170.90x) |
| cycles | 68627.3 (1.00x) | 54.3 (1264.55x) | 63.4 (1082.79x) | 44.0 (1558.65x) | 43.5 (1579.45x) |
| types | 183.7 (1.00x) | 88.1 (2.08x) | 84.7 (2.17x) | 264.9 (0.69x) | 73.8 (2.49x) |

#### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 27.5 (1.00x) | 3.3 (8.25x) | 15.8 (1.74x) | 5.8 (4.72x) | 5.8 (4.71x) |
| bulk | 84997.9 (1.00x) | 36.8 (2307.22x) | 55.1 (1541.21x) | 34.3 (2475.91x) | 34.3 (2476.63x) |
| mixed | 82786.0 (1.00x) | 66.3 (1247.91x) | 92.8 (892.19x) | 56.8 (1457.24x) | 56.2 (1473.06x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 156.6 (1.00x) | 43.7 (3.58x) | 63.5 (2.46x) | 255.8 (0.61x) | 46.9 (3.34x) |

#### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 32.4 (1.00x) | 4.8 (6.78x) | 18.1 (1.79x) | 9.8 (3.29x) | 9.5 (3.42x) |
| bulk | 137483.7 (1.00x) | 49.8 (2761.83x) | 65.7 (2092.28x) | 51.0 (2694.70x) | 47.8 (2876.23x) |
| mixed | 97382.0 (1.00x) | 77.3 (1259.14x) | 107.2 (908.75x) | 72.0 (1353.28x) | 74.3 (1310.84x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 169.4 (1.00x) | 58.0 (2.92x) | 76.0 (2.23x) | 270.5 (0.63x) | 62.6 (2.71x) |

### What thread support costs

Time, ns per operation, with thread support / with `BOOST_DISABLE_THREADS` (ratio):

`allocator alone`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 43.2 / 27.3 (1.58x) | 17.5 / 3.3 (5.31x) | 15.8 / 15.5 (1.02x) | 16.8 / 4.2 (4.05x) | 16.9 / 3.9 (4.30x) |
| raw_batch | 76550.6 / 70657.7 (1.08x) | 25.7 / 23.3 (1.10x) | 28.2 / 28.0 (1.01x) | 25.3 / 21.3 (1.19x) | 24.5 / 21.4 (1.14x) |

`boost::root_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 78.3 / 36.4 (2.15x) | 57.5 / 11.1 (5.19x) | 53.6 / 20.2 (2.66x) | 57.0 / 9.4 (6.05x) | 57.8 / 9.5 (6.11x) |
| bulk | 146740.8 / 141721.0 (1.04x) | 113.5 / 69.8 (1.63x) | 115.9 / 79.2 (1.46x) | 105.3 / 65.8 (1.60x) | 106.5 / 65.7 (1.62x) |
| mixed | 100748.9 / 99023.1 (1.02x) | 142.1 / 96.8 (1.47x) | 155.9 / 119.2 (1.31x) | 131.9 / 84.9 (1.55x) | 132.7 / 84.6 (1.57x) |
| cycles | 68706.2 / 68627.3 (1.00x) | 129.6 / 54.3 (2.39x) | 136.8 / 63.4 (2.16x) | 119.8 / 44.0 (2.72x) | 122.1 / 43.5 (2.81x) |
| types | 232.6 / 183.7 (1.27x) | 135.3 / 88.1 (1.54x) | 136.5 / 84.7 (1.61x) | 342.3 / 264.9 (1.29x) | 128.1 / 73.8 (1.74x) |

`std::unique_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.9 / 27.5 (1.71x) | 26.1 / 3.3 (7.84x) | 15.3 / 15.8 (0.97x) | 18.0 / 5.8 (3.09x) | 17.6 / 5.8 (3.03x) |
| bulk | 84289.7 / 84997.9 (0.99x) | 52.7 / 36.8 (1.43x) | 55.6 / 55.1 (1.01x) | 40.1 / 34.3 (1.17x) | 40.2 / 34.3 (1.17x) |
| mixed | 80728.8 / 82786.0 (0.98x) | 74.3 / 66.3 (1.12x) | 91.0 / 92.8 (0.98x) | 59.8 / 56.8 (1.05x) | 60.2 / 56.2 (1.07x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 179.6 / 156.6 (1.15x) | 60.7 / 43.7 (1.39x) | 62.3 / 63.5 (0.98x) | 247.2 / 255.8 (0.97x) | 58.9 / 46.9 (1.26x) |

`std::shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 62.1 / 32.4 (1.92x) | 43.8 / 4.8 (9.15x) | 17.7 / 18.1 (0.97x) | 19.8 / 9.8 (2.01x) | 19.6 / 9.5 (2.07x) |
| bulk | 138642.1 / 137483.7 (1.01x) | 79.7 / 49.8 (1.60x) | 64.4 / 65.7 (0.98x) | 54.6 / 51.0 (1.07x) | 51.7 / 47.8 (1.08x) |
| mixed | 98903.4 / 97382.0 (1.02x) | 108.4 / 77.3 (1.40x) | 111.6 / 107.2 (1.04x) | 77.9 / 72.0 (1.08x) | 78.5 / 74.3 (1.06x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 195.1 / 169.4 (1.15x) | 95.1 / 58.0 (1.64x) | 73.7 / 76.0 (0.97x) | 258.4 / 270.5 (0.96x) | 71.4 / 62.6 (1.14x) |

## Analysis

**`boost::pool_allocator`.** It keeps its free list sorted by address, so each `deallocate` is a
linear search. That doesn't show when one object is allocated and freed at a time (`raw_churn`,
`churn`: the list stays short), but every scenario that releases many objects together pays per
object in proportion to the number already free. The effect is the same for all three pointer types
and in both builds: in the scaling tables the cost per object goes from 14–21 µs at 12,500 objects to
180–348 µs at 200,000, while every other allocator stays flat. Releasing a `node_proxy` scope with
many nodes is exactly what transformed programs do, which is why the default was changed.

**Page allocators vs. `fast_pool_allocator` and `std::allocator`.** All three avoid the sorted list
and free in constant time. In the release scenarios the page allocators are the fastest in both
builds, up to 1.7× faster than `std` and 1.5× faster than `fast`. They also use the least memory: 9–18%
less than `pool` and `fast`, and up to 18% less than `std` (the same as `std` for `root_ptr` in
`bulk`). The Boost pools grow in doubling chunks, so part of their last chunk is reserved but unused;
the page allocators grow one 64 KiB page at a time and have no per-block header. For churn,
`std::allocator` wins with thread support, because glibc serves it from a per-thread cache while a
page pool always takes its mutex. Without thread support, `fast_pool_allocator` wins churn for the
standard pointers (3.3 vs. 5.8 ns per `unique_ptr`); I haven't profiled where the page pool's extra
2.5 ns go.

**By type vs. by size.** With one to three types (`bulk`, `mixed`) the two policies behave the same,
because each type has its own size class anyway. With many types of the same size (`types`),
`by_type` opens one 64 KiB page per type. Building a page's free list writes into every block, so the
whole page becomes resident even when it holds a handful of objects: 1.6–1.9 MB more than `by_size`
and 2.7× to 4.2× the time. `by_type` pays off only when keeping types apart matters more than
footprint, for example to confine a use-after-free of one type to that type's pages.

**Thread support.** Without `BOOST_DISABLE_THREADS`, every `root_ptr` operation locks a global
recursive mutex, node reference counts are atomic, and the pools lock their own mutexes. Together
that is 6.1× on `root_ptr` churn with `page_allocator_by_size` (57.8 vs. 9.5 ns), 1.6× on bulk and
mixed release and 2.8× on cycles. About 12 ns of the churn difference is the page pool's mutex (the
same drop as `unique_ptr` churn, 17.6 to 5.8 ns); the rest is `root_ptr`'s own mutex and atomic
counts. The allocator-only rows show the pools' share: 4.3× on `raw_churn` for the page allocators
and 5.3× for `fast`, but 1.0× for `std`.

**Threads.** For `unique_ptr` and `shared_ptr`, glibc's per-thread caches make `std::allocator` 28–36×
faster than any pool here: all four threads allocate the same type, so they contend on a single page
pool's mutex (or Boost's singleton-pool mutex). For `root_ptr` the gap shrinks to 1.5×, because
`root_ptr` itself takes a global recursive mutex on every operation.

**Pointer types.** With `page_allocator_by_size` and thread support, `root_ptr` costs 3.3× `unique_ptr`
and 3.0× `shared_ptr` per churn, and 2.6× / 2.1× in bulk. Without thread support the gap narrows to
1.6× `unique_ptr` and 1.0× `shared_ptr` per churn, and 1.9× / 1.4× in bulk: most of `root_ptr`'s extra
cost is locking. The per-object state explains the rest: a 72-byte node and a 32-byte handle (the
handle's links in its `node_proxy` root set), against 48 + 8 bytes for `unique_ptr` and 64 + 16 for
`shared_ptr`, which matches the measured memory (12.7 MB vs. 7.1 MB and 9.6 MB for 100,000 objects).
What it buys is the `cycles` row: `root_ptr` reclaims cycles at 44–122 ns per node, which neither
standard pointer can do.

## Recommendations

1. **Change the default node allocator. Done 2026-09-27:** the default is now
   `page_allocator_by_size`, which removes the quadratic release and also saves memory.
   `-D BOOST_ROOT_PTR_ALLOCATOR=...` still selects another allocator per build.
2. **Build single-threaded programs with `BOOST_DISABLE_THREADS`.** It makes `root_ptr` up to 6×
   faster here. `fcxxss.sh` doesn't define it, so today a user has to pass it; an option in
   `fcxxss.sh` would make it discoverable.
3. **Carve pages lazily.** `page_pool::grow()` threads the free list through a whole page up front,
   touching all of it. Handing out blocks from a bump pointer and only threading freed blocks would
   make a sparsely used page cost only what it holds, which is what hurts `page_allocator_by_type`.
4. **Add per-thread caches to the page allocators**, in front of the shared pools. That is what makes
   `std::allocator` win under contention and on churn with thread support.
5. **Lock per `node_proxy` ring instead of globally** in `root_ptr` (discussed separately). That
   would recover part of the thread-support cost for multi-threaded programs, which cannot use
   `BOOST_DISABLE_THREADS`.

## Reproducing

```sh
bench/run.sh 5            # measure both builds and print the tables (about 25 minutes)
bench/run.sh --tables     # reprint the tables from bench/raw-main.txt and bench/raw-scale.txt
bench/plot.py             # draw the charts from the same files into bench/ALLOCATOR_BENCHMARK.pdf
# a single measurement:
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    bench/allocbench.cpp -o allocbench -lboost_thread -lpthread      # add -DBOOST_DISABLE_THREADS for the other build
./allocbench shared page_size bulk     # <pointer> <allocator> <scenario> [n]
```

The raw measurements behind these tables are in `bench/raw-main.txt` (one line per run: build,
pointer, allocator, scenario, ns, kB) and `bench/raw-scale.txt`. The complete output of the run, with
start and end load averages, is in `bench/bench-run.log`.
