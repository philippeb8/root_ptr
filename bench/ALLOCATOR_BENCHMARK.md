# Node allocator benchmark: page allocators vs. the default pool

> **Update 2026-09-27:** following this benchmark, the default node allocator is now
> `boost::page_allocator_by_size` (`BOOST_ROOT_PTR_ALLOCATOR` in `detail/node_base.hpp`). In the
> tables, `pool` is `boost::pool_allocator`, the default when the benchmark was first run;
> `-D BOOST_ROOT_PTR_ALLOCATOR=boost::pool_allocator` restores it.

Measured 2026-09-27 with `bench/allocbench.cpp` and `bench/run.sh`, for three smart pointer types
(`boost::root_ptr`, `std::unique_ptr`, `std::shared_ptr`) and three builds: with thread support (the
default), with `BOOST_DISABLE_THREADS`, and with `BOOST_ROOT_PTR_STRIPED_LOCKS`. The same results as
charts: `bench/ALLOCATOR_BENCHMARK.pdf` (drawn by `bench/plot.py`).

## Summary

- **`boost::pool_allocator` is quadratic when many objects are released.** Its `deallocate` calls
  `ordered_free`, which walks the free list to keep it sorted (`simple_segregated_storage::find_prev`
  in Boost.Pool). This does not depend on the pointer type or the build: releasing 200,000 objects at
  once costs 204–425 µs per object, and the cost per object grows ×1.8 to ×2.9 each time the count
  doubles. Every other allocator stays flat, at 39–150 ns per object (single runs, with a few spikes up
  to 241 ns).
- **The page allocators are the fastest in every bulk, mixed-size and cycle release, in both
  single-threaded builds** (`by_size` within 0–13% of `by_type`). Compared with `pool` they are 550×
  to 3,000× faster there, and `page_allocator_by_size` holds the same objects in 9–18% less memory.
  Churn (one object created and dropped at a time) is the exception:
  - with thread support, `std::allocator` is 5–14% faster (glibc's per-thread cache takes no lock,
    while a page pool takes its mutex);
  - without thread support, `fast_pool_allocator` is 1.8–2× faster for `unique_ptr` and `shared_ptr`;
    for `root_ptr` the page allocators stay fastest.
- **Thread support is the largest single cost for `root_ptr`.** Built with `BOOST_DISABLE_THREADS`,
  `root_ptr` churn drops from 59.6 to 10.3 ns (5.8×), bulk release from 108.6 to 63.5 ns and cycles from
  125.9 to 51.5 ns per node, with `page_allocator_by_size`. The saving is the global `root_ptr` mutex,
  atomic reference counts and the page pool's mutex. `std::allocator` alone and with the standard
  pointers stays within run-to-run noise (0.9–1.1×): glibc and libstdc++ ignore the macro.
- **Striped locks (`BOOST_ROOT_PTR_STRIPED_LOCKS`) trade single-threaded speed for scaling.** With
  four threads and `std::allocator`, `root_ptr` goes from 305 to 31 ns per object (9.8×); with the pool
  allocators only 1.3–1.4×, because their own mutex then serializes the threads. Single-threaded,
  every scenario is 1.25× to 3.3× slower: about 8 lock operations per object instead of 3. The option
  stays off by default.
- **`root_ptr` against the standard pointers:** with thread support it costs 3.3× `unique_ptr` and
  2.9× `shared_ptr` per churn. Without thread support it costs 1.7× `unique_ptr` and 1.1× `shared_ptr`,
  in 1.8× the memory of `unique_ptr`. It is the only one of the three that reclaims cycles.
- **`page_allocator_by_type` costs more with many sparse types:** 32 types of 100 objects each take
  2.5× to 4.8× the time of `by_size` and 1.6–1.8 MB more memory. Each type fills a 64 KiB page of its
  own, and a new page is fully touched when its free list is built.
- **With four threads, `std::allocator` is far faster for `unique_ptr` and `shared_ptr`**: 4–5 ns
  per object, 34–38× faster than `page_allocator_by_size` (167–172 ns). glibc's per-thread caches take
  no lock, while each page pool has one mutex. With `root_ptr` and its global lock the gap is 1.3×
  (305 vs. 406 ns).

## Setup

| | |
|---|---|
| CPU | Intel Core i7-4700HQ, 4 cores / 8 threads, 2.4 GHz |
| Memory | 16 GB |
| OS | Linux 5.15 (Ubuntu 22.04) |
| Compiler | clang 23 (`-std=c++20 -O2 -DNDEBUG`), libstdc++ 12, glibc 2.35, Boost.Pool from `/usr/include` |
| Library | `/opt/fornux/superset/usr/include` (root_ptr.hpp, node_base.hpp and page_allocator.hpp as of 2026-09-27) |
| Load | Shared desktop machine (browser, Xorg); load average 1.2 at the start and 1.8 at the end of the run |

Builds. `run.sh` compiles `allocbench.cpp` three times:

| build | defines | what changes |
|---|---|---|
| with thread support (default) | none | `root_ptr` locks a global recursive mutex on every operation; node reference counts are atomic; Boost pools and page pools lock a mutex |
| without thread support | `BOOST_DISABLE_THREADS` | no `root_ptr` lock; plain reference counts (`sp_counted_base_nt`); Boost pools use `null_mutex`; page pools use a no-op mutex. `std::allocator` (glibc) and `std::shared_ptr`'s counts (libstdc++) do not change. Single-threaded scenarios only |
| striped locks | `BOOST_ROOT_PTR_STRIPED_LOCKS` | `root_ptr`'s global mutex is replaced by 1024 spin locks indexed by root address: a root's ring links and pointers are guarded by its stripe, a ring change takes the stripes of the roots involved and of their neighbours, and a node is released only after every stripe is unlocked. Measured for `root_ptr` only, which is the only pointer that uses them |

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
| raw_churn | 42.1 (1.00x) | 18.5 (2.27x) | 16.5 (2.55x) | 17.2 (2.45x) | 16.8 (2.51x) |
| raw_batch | 73293.4 (1.00x) | 26.6 (2752.29x) | 28.2 (2601.83x) | 24.7 (2966.14x) | 24.8 (2953.00x) |

#### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 84.0 (1.00x) | 58.4 (1.44x) | 56.7 (1.48x) | 59.3 (1.42x) | 59.6 (1.41x) |
| bulk | 148436.6 (1.00x) | 115.6 (1284.05x) | 120.6 (1231.12x) | 108.8 (1363.68x) | 108.6 (1367.07x) |
| mixed | 105470.6 (1.00x) | 140.9 (748.66x) | 158.5 (665.60x) | 134.5 (784.17x) | 137.7 (765.78x) |
| cycles | 69912.6 (1.00x) | 128.2 (545.13x) | 140.4 (498.06x) | 123.7 (565.00x) | 125.9 (555.35x) |
| threads | 455.2 (1.00x) | 399.4 (1.14x) | 305.2 (1.49x) | 410.1 (1.11x) | 406.2 (1.12x) |
| types | 236.1 (1.00x) | 133.2 (1.77x) | 124.3 (1.90x) | 332.0 (0.71x) | 132.7 (1.78x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13948 | 14036 | 12736 | 12708 | 12712 |
| mixed | 27984 | 28048 | 24360 | 23824 | 23800 |
| types | 2140 | 2192 | 2260 | 3908 | 2296 |

#### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.6 (1.00x) | 26.0 (1.79x) | 15.8 (2.96x) | 17.4 (2.68x) | 18.0 (2.59x) |
| bulk | 86378.1 (1.00x) | 56.8 (1521.01x) | 54.7 (1578.84x) | 41.5 (2082.91x) | 40.8 (2117.11x) |
| mixed | 92654.9 (1.00x) | 79.4 (1166.79x) | 102.3 (905.63x) | 62.5 (1482.72x) | 61.9 (1496.61x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 298.6 (1.00x) | 245.7 (1.22x) | 4.3 (68.64x) | 190.3 (1.57x) | 167.4 (1.78x) |
| types | 175.1 (1.00x) | 68.5 (2.55x) | 69.5 (2.52x) | 275.4 (0.64x) | 58.0 (3.02x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 8464 | 8840 | 8676 | 7184 | 7176 |
| mixed | 19824 | 19920 | 17640 | 16156 | 16228 |
| types | 1864 | 1888 | 1884 | 3800 | 2004 |

#### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 63.6 (1.00x) | 46.7 (1.36x) | 18.4 (3.45x) | 19.9 (3.19x) | 20.8 (3.05x) |
| bulk | 145716.6 (1.00x) | 86.1 (1692.41x) | 66.4 (2195.52x) | 56.2 (2591.44x) | 55.5 (2627.89x) |
| mixed | 97321.1 (1.00x) | 108.8 (894.08x) | 105.8 (919.86x) | 80.1 (1215.00x) | 80.1 (1215.15x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| threads | 388.5 (1.00x) | 302.3 (1.29x) | 5.1 (76.63x) | 168.3 (2.31x) | 171.6 (2.26x) |
| types | 204.5 (1.00x) | 92.5 (2.21x) | 78.8 (2.60x) | 263.3 (0.78x) | 79.0 (2.59x) |

Resident memory grown while the objects are live, kB:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 11480 | 11468 | 11040 | 9564 | 9548 |
| mixed | 24068 | 24156 | 21100 | 19856 | 19900 |
| types | 1928 | 1960 | 1944 | 3776 | 1980 |

#### Pointer types side by side

Time, ns per operation, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 84.0 / 46.6 / 63.6 | 58.4 / 26.0 / 46.7 | 56.7 / 15.8 / 18.4 | 59.3 / 17.4 / 19.9 | 59.6 / 18.0 / 20.8 |
| bulk | 148436.6 / 86378.1 / 145716.6 | 115.6 / 56.8 / 86.1 | 120.6 / 54.7 / 66.4 | 108.8 / 41.5 / 56.2 | 108.6 / 40.8 / 55.5 |
| mixed | 105470.6 / 92654.9 / 97321.1 | 140.9 / 79.4 / 108.8 | 158.5 / 102.3 / 105.8 | 134.5 / 62.5 / 80.1 | 137.7 / 61.9 / 80.1 |
| cycles | 69912.6 / n/a / n/a | 128.2 / n/a / n/a | 140.4 / n/a / n/a | 123.7 / n/a / n/a | 125.9 / n/a / n/a |
| threads | 455.2 / 298.6 / 388.5 | 399.4 / 245.7 / 302.3 | 305.2 / 4.3 / 5.1 | 410.1 / 190.3 / 168.3 | 406.2 / 167.4 / 171.6 |
| types | 236.1 / 175.1 / 204.5 | 133.2 / 68.5 / 92.5 | 124.3 / 69.5 / 78.8 | 332.0 / 275.4 / 263.3 | 132.7 / 58.0 / 79.0 |

Resident memory grown, kB, `root_ptr` / `unique_ptr` / `shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| bulk | 13948 / 8464 / 11480 | 14036 / 8840 / 11468 | 12736 / 8676 / 11040 | 12708 / 7184 / 9564 | 12712 / 7176 / 9548 |
| mixed | 27984 / 19824 / 24068 | 28048 / 19920 / 24156 | 24360 / 17640 / 21100 | 23824 / 16156 / 19856 | 23800 / 16228 / 19900 |
| types | 2140 / 1864 / 1928 | 2192 / 1888 / 1960 | 2260 / 1884 / 1944 | 3908 / 3800 / 3776 | 2296 / 2004 / 1980 |

#### Bulk release scaling

ns per object, one run per size:

`boost::root_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 20365.0 | 113.0 | 118.4 | 110.9 | 103.7 |
| 25000 | 37117.4 | 118.9 | 117.5 | 113.7 | 107.8 |
| 50000 | 71948.8 | 117.2 | 130.7 | 189.9 | 241.2 |
| 100000 | 148910.2 | 150.6 | 137.2 | 136.4 | 126.2 |
| 200000 | 425064.2 | 188.6 | 141.0 | 113.6 | 114.8 |

`std::unique_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 15265.3 | 52.4 | 58.4 | 43.8 | 60.7 |
| 25000 | 27483.5 | 57.8 | 64.6 | 44.9 | 42.2 |
| 50000 | 49452.0 | 55.5 | 89.4 | 40.4 | 46.9 |
| 100000 | 100986.8 | 58.5 | 54.4 | 38.8 | 46.1 |
| 200000 | 203540.4 | 51.3 | 52.3 | 38.9 | 39.7 |

`std::shared_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12500 | 20124.2 | 89.4 | 75.4 | 54.9 | 56.2 |
| 25000 | 36038.8 | 86.2 | 71.8 | 54.1 | 54.9 |
| 50000 | 68131.1 | 78.6 | 75.3 | 52.6 | 54.5 |
| 100000 | 143460.2 | 85.6 | 75.7 | 53.1 | 54.9 |
| 200000 | 327982.0 | 91.3 | 64.7 | 52.2 | 52.0 |

### Built with `BOOST_DISABLE_THREADS`

No `root_ptr` mutex, plain reference counts, no pool mutexes; single-threaded scenarios only.

#### Allocator alone

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 27.9 (1.00x) | 3.4 (8.14x) | 16.2 (1.72x) | 4.2 (6.67x) | 4.1 (6.76x) |
| raw_batch | 73126.1 (1.00x) | 23.2 (3150.63x) | 28.5 (2567.63x) | 21.6 (3393.32x) | 21.7 (3374.53x) |

#### `boost::root_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 36.9 (1.00x) | 10.9 (3.39x) | 20.6 (1.79x) | 9.6 (3.85x) | 10.3 (3.59x) |
| bulk | 145032.4 (1.00x) | 71.5 (2027.86x) | 79.7 (1819.73x) | 64.6 (2245.08x) | 63.5 (2284.33x) |
| mixed | 99895.4 (1.00x) | 99.7 (1001.96x) | 118.9 (840.09x) | 87.2 (1146.25x) | 89.9 (1111.68x) |
| cycles | 69638.6 (1.00x) | 55.7 (1250.92x) | 67.2 (1035.83x) | 45.7 (1522.49x) | 51.5 (1352.73x) |
| types | 199.1 (1.00x) | 89.6 (2.22x) | 91.3 (2.18x) | 271.2 (0.73x) | 77.4 (2.57x) |

#### `std::unique_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 28.4 (1.00x) | 3.4 (8.44x) | 16.3 (1.74x) | 6.0 (4.72x) | 6.1 (4.69x) |
| bulk | 86103.5 (1.00x) | 37.6 (2291.81x) | 55.7 (1545.84x) | 36.7 (2348.70x) | 36.2 (2377.89x) |
| mixed | 85834.4 (1.00x) | 63.8 (1345.16x) | 91.9 (933.59x) | 57.7 (1487.34x) | 57.3 (1497.72x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 147.7 (1.00x) | 42.7 (3.46x) | 60.7 (2.43x) | 252.0 (0.59x) | 52.6 (2.81x) |

#### `std::shared_ptr`

Time, ns per operation (speed-up over `pool` in parentheses):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 31.4 (1.00x) | 4.8 (6.54x) | 18.0 (1.74x) | 10.4 (3.02x) | 9.3 (3.35x) |
| bulk | 147390.1 (1.00x) | 52.0 (2835.52x) | 72.5 (2034.37x) | 48.4 (3043.36x) | 48.5 (3035.84x) |
| mixed | 103331.8 (1.00x) | 76.0 (1360.34x) | 106.3 (971.89x) | 71.2 (1451.29x) | 76.3 (1354.11x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 173.1 (1.00x) | 59.3 (2.92x) | 82.5 (2.10x) | 305.5 (0.57x) | 67.9 (2.55x) |

### Striped locks (`BOOST_ROOT_PTR_STRIPED_LOCKS`)

`boost::root_ptr` only. Time, ns per operation, global lock / striped locks (ratio: above 1.00x the striped build is faster):

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 84.0 / 138.8 (0.61x) | 58.4 / 119.3 (0.49x) | 56.7 / 117.2 (0.48x) | 59.3 / 116.7 (0.51x) | 59.6 / 119.8 (0.50x) |
| bulk | 148436.6 / 148502.1 (1.00x) | 115.6 / 206.5 (0.56x) | 120.6 / 215.8 (0.56x) | 108.8 / 205.4 (0.53x) | 108.6 / 207.9 (0.52x) |
| mixed | 105470.6 / 103994.2 (1.01x) | 140.9 / 231.2 (0.61x) | 158.5 / 235.6 (0.67x) | 134.5 / 227.3 (0.59x) | 137.7 / 219.3 (0.63x) |
| cycles | 69912.6 / 70362.6 (0.99x) | 128.2 / 398.6 (0.32x) | 140.4 / 402.0 (0.35x) | 123.7 / 412.4 (0.30x) | 125.9 / 410.1 (0.31x) |
| threads | 455.2 / 339.5 (1.34x) | 399.4 / 292.1 (1.37x) | 305.2 / 31.3 (9.75x) | 410.1 / 289.7 (1.42x) | 406.2 / 286.1 (1.42x) |
| types | 236.1 / 345.8 (0.68x) | 133.2 / 247.4 (0.54x) | 124.3 / 247.2 (0.50x) | 332.0 / 414.2 (0.80x) | 132.7 / 234.8 (0.57x) |

### What thread support costs

Time, ns per operation, with thread support / with `BOOST_DISABLE_THREADS` (ratio):

`allocator alone`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| raw_churn | 42.1 / 27.9 (1.51x) | 18.5 / 3.4 (5.40x) | 16.5 / 16.2 (1.02x) | 17.2 / 4.2 (4.10x) | 16.8 / 4.1 (4.06x) |
| raw_batch | 73293.4 / 73126.1 (1.00x) | 26.6 / 23.2 (1.15x) | 28.2 / 28.5 (0.99x) | 24.7 / 21.6 (1.15x) | 24.8 / 21.7 (1.15x) |

`boost::root_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 84.0 / 36.9 (2.28x) | 58.4 / 10.9 (5.37x) | 56.7 / 20.6 (2.75x) | 59.3 / 9.6 (6.19x) | 59.6 / 10.3 (5.79x) |
| bulk | 148436.6 / 145032.4 (1.02x) | 115.6 / 71.5 (1.62x) | 120.6 / 79.7 (1.51x) | 108.8 / 64.6 (1.68x) | 108.6 / 63.5 (1.71x) |
| mixed | 105470.6 / 99895.4 (1.06x) | 140.9 / 99.7 (1.41x) | 158.5 / 118.9 (1.33x) | 134.5 / 87.2 (1.54x) | 137.7 / 89.9 (1.53x) |
| cycles | 69912.6 / 69638.6 (1.00x) | 128.2 / 55.7 (2.30x) | 140.4 / 67.2 (2.09x) | 123.7 / 45.7 (2.71x) | 125.9 / 51.5 (2.45x) |
| types | 236.1 / 199.1 (1.19x) | 133.2 / 89.6 (1.49x) | 124.3 / 91.3 (1.36x) | 332.0 / 271.2 (1.22x) | 132.7 / 77.4 (1.72x) |

`std::unique_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 46.6 / 28.4 (1.64x) | 26.0 / 3.4 (7.72x) | 15.8 / 16.3 (0.97x) | 17.4 / 6.0 (2.89x) | 18.0 / 6.1 (2.97x) |
| bulk | 86378.1 / 86103.5 (1.00x) | 56.8 / 37.6 (1.51x) | 54.7 / 55.7 (0.98x) | 41.5 / 36.7 (1.13x) | 40.8 / 36.2 (1.13x) |
| mixed | 92654.9 / 85834.4 (1.08x) | 79.4 / 63.8 (1.24x) | 102.3 / 91.9 (1.11x) | 62.5 / 57.7 (1.08x) | 61.9 / 57.3 (1.08x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 175.1 / 147.7 (1.19x) | 68.5 / 42.7 (1.61x) | 69.5 / 60.7 (1.14x) | 275.4 / 252.0 (1.09x) | 58.0 / 52.6 (1.10x) |

`std::shared_ptr`:

| scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| churn | 63.6 / 31.4 (2.03x) | 46.7 / 4.8 (9.75x) | 18.4 / 18.0 (1.02x) | 19.9 / 10.4 (1.92x) | 20.8 / 9.3 (2.23x) |
| bulk | 145716.6 / 147390.1 (0.99x) | 86.1 / 52.0 (1.66x) | 66.4 / 72.5 (0.92x) | 56.2 / 48.4 (1.16x) | 55.5 / 48.5 (1.14x) |
| mixed | 97321.1 / 103331.8 (0.94x) | 108.8 / 76.0 (1.43x) | 105.8 / 106.3 (1.00x) | 80.1 / 71.2 (1.12x) | 80.1 / 76.3 (1.05x) |
| cycles | n/a | n/a | n/a | n/a | n/a |
| types | 204.5 / 173.1 (1.18x) | 92.5 / 59.3 (1.56x) | 78.8 / 82.5 (0.95x) | 263.3 / 305.5 (0.86x) | 79.0 / 67.9 (1.16x) |

## Analysis

**`boost::pool_allocator`.** It keeps its free list sorted by address, so each `deallocate` is a
linear search. That doesn't show when one object is allocated and freed at a time (`raw_churn`,
`churn`: the list stays short), but every scenario that releases many objects together pays per
object in proportion to the number already free. The effect is the same for all three pointer types
and in every build: in the scaling tables the cost per object goes from 15–20 µs at 12,500 objects to
204–425 µs at 200,000, while every other allocator stays flat. Releasing a `node_proxy` scope with
many nodes is exactly what transformed programs do, which is why the default was changed.

**Page allocators vs. `fast_pool_allocator` and `std::allocator`.** All three avoid the sorted list
and free in constant time. In the release scenarios a page allocator is the fastest in both
single-threaded builds. They also use the least memory: 9–18% less than `pool` and `fast`, and up to 17%
less than `std` (the same as `std` for `root_ptr` in `bulk`). The Boost pools grow in doubling chunks,
so part of their last chunk is reserved but unused; the page allocators grow one 64 KiB page at a time
and have no per-block header. For churn, `std::allocator` wins with thread support, because glibc
serves it from a per-thread cache while a page pool always takes its mutex. Without thread support,
`fast_pool_allocator` wins churn and "many types" for the standard pointers (3.4 vs. 6.1 ns per
`unique_ptr` churn); I haven't profiled where the page pool's extra time goes.

**By type vs. by size.** With one to three types (`bulk`, `mixed`) the two policies behave the same,
because each type has its own size class anyway. With many types of the same size (`types`),
`by_type` opens one 64 KiB page per type. Building a page's free list writes into every block, so the
whole page becomes resident even when it holds a handful of objects: 1.6–1.8 MB more than `by_size`
and 2.5× to 4.8× the time. `by_type` pays off only when keeping types apart matters more than
footprint, for example to confine a use-after-free of one type to that type's pages.

**Thread support.** With thread support, every `root_ptr` operation locks a global recursive mutex,
node reference counts are atomic, and the pools lock their own mutexes. Together that is 5.8× on
`root_ptr` churn with `page_allocator_by_size` (59.6 vs. 10.3 ns), 1.5–1.7× on bulk and mixed release
and 2.4× on cycles. About 12 ns of the churn difference is the page pool's mutex (the same drop as
`unique_ptr` churn, 18.0 to 6.1 ns); the rest is `root_ptr`'s own mutex and atomic counts. The
allocator-only rows show the pools' share: 4.1× on `raw_churn` for `page_size` and 5.4× for `fast`,
but 1.0× for `std`.

**Striped locks.** With `BOOST_ROOT_PTR_STRIPED_LOCKS`, `root_ptr` no longer serializes every
operation of every thread on one mutex. The price is more lock operations: a construction, access
and destruction takes about 8 instead of 3, so every single-threaded scenario is 1.25× to 3.3× slower
(except with `pool`, whose quadratic release dominates everything). With four threads the gain depends
on the allocator: 9.8× with `std::allocator`, whose per-thread caches take no lock, but only 1.3–1.4×
with the pool allocators, whose own mutex becomes the bottleneck. Striped `root_ptr` with
`std::allocator` (31 ns) is still 6–7× slower than `unique_ptr` and `shared_ptr` with the same allocator
(4–5 ns) in that scenario. Packing the locks into fewer cache lines did not help single-threaded code
and made four threads 2× slower through false sharing.

**Threads.** For `unique_ptr` and `shared_ptr`, glibc's per-thread caches make `std::allocator` 34–38×
faster than `page_allocator_by_size` here: all four threads allocate the same type, so they contend
on a single page pool's mutex (or Boost's singleton-pool mutex). For `root_ptr` with its default
global lock the gap shrinks to 1.3×, because `root_ptr` itself serializes every operation.

**Pointer types.** With `page_allocator_by_size` and thread support, `root_ptr` costs 3.3× `unique_ptr`
and 2.9× `shared_ptr` per churn, and 2.7× / 2.0× in bulk. Without thread support the gap narrows to
1.7× `unique_ptr` and 1.1× `shared_ptr` per churn, and 1.75× / 1.3× in bulk: most of `root_ptr`'s extra
cost is locking. The per-object state explains the rest: a 72-byte node and a 32-byte handle (the
handle's links in its `node_proxy` ring), against 48 + 8 bytes for `unique_ptr` and 64 + 16 for
`shared_ptr`, which matches the measured memory (12.7 MB vs. 7.2 MB and 9.5 MB for 100,000 objects).
What it buys is the `cycles` row: `root_ptr` reclaims cycles at 52–126 ns per node, which neither
standard pointer can do.

## Recommendations

1. **Change the default node allocator. Done 2026-09-27:** the default is now
   `page_allocator_by_size`, which removes the quadratic release and also saves memory.
   `-D BOOST_ROOT_PTR_ALLOCATOR=...` still selects another allocator per build.
2. **Build single-threaded programs with `BOOST_DISABLE_THREADS`.** It makes `root_ptr` up to 5.8×
   faster here. **Done 2026-09-27:** `FCXXSS_NO_THREADS=1 fcxxss.sh ...` defines it in all three
   passes, with a precompiled header of its own. The program must not use threads at all.
3. **Carve pages lazily.** `page_pool::grow()` threads the free list through a whole page up front,
   touching all of it. Handing out blocks from a bump pointer and only threading freed blocks would
   make a sparsely used page cost only what it holds, which is what hurts `page_allocator_by_type`.
4. **Add per-thread caches to the page allocators**, in front of the shared pools. That is what makes
   `std::allocator` win under contention and on churn with thread support, and it is what striped
   locks need to scale with the page allocators (1.4× today, against 9.8× with `std::allocator`).
5. **Finer-grained locking in `root_ptr`. Implemented 2026-09-27 as an option,
   `BOOST_ROOT_PTR_STRIPED_LOCKS`.** Rings are not one per `node_proxy` (a copy moves its source into a
   ring of its own, an assignment moves the source into the target's ring), so the locks are striped
   by root address instead. It pays off only with several threads, so it stays off by default.

## Reproducing

```sh
bench/run.sh 5            # measure all three builds and print the tables (about 30 minutes)
bench/run.sh --tables     # reprint the tables from bench/raw-main.txt and bench/raw-scale.txt
bench/plot.py             # draw the charts from the same files into bench/ALLOCATOR_BENCHMARK.pdf
# a single measurement:
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    bench/allocbench.cpp -o allocbench -lboost_thread -lpthread
    # add -DBOOST_DISABLE_THREADS or -DBOOST_ROOT_PTR_STRIPED_LOCKS for the other builds
./allocbench shared page_size bulk     # <pointer> <allocator> <scenario> [n]
```

The raw measurements behind these tables are in `bench/raw-main.txt` (one line per run: build,
pointer, allocator, scenario, ns, kB) and `bench/raw-scale.txt`. The complete output of the run, with
start and end load averages, is in `bench/bench-run.log`.
