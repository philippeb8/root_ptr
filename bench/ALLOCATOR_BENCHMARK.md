# Node allocator benchmark: the whole matrix

Every build (macro combination) x allocator x pointer type x scenario, measured in one run with `bench/allocbench.cpp` and `bench/run.sh`; this file and `ALLOCATOR_BENCHMARK.pdf` (the same matrices as colour pages) are written by `bench/report.py`. The previous report (2026-09-27/28, measured in separate runs) is kept in `bench/archive-2026-09-27/`.

## Summary

- The default allocator, page_allocator_by_size, is the fastest or within 10% of the fastest in 114 of the 140 measured (build, pointer, scenario) cells. It loses by a wide margin only with 4 threads and no thread caches, where every allocation takes the page pool's mutex: std::allocator (glibc, per-thread arenas) is 36x faster for unique_ptr (5.0 vs. 182.9 ns), 28x for shared_ptr, 19x for shared_node_ptr, and 1.24x for root_ptr behind its global lock (313 vs. 389 ns).
- boost::pool_allocator is quadratic when many objects are released: its deallocate keeps the free list sorted (ordered_free). Releasing 100,000 objects together costs 85-144 us per object for every pointer and build, and about 2x more each time the count doubles (189-402 us at 200,000). Every other allocator stays at 41-255 ns in every build. The allocator alone shows it too: a batch costs 70.7 us per block against 28.9 ns.
- Thread support (the default) is root_ptr's largest single cost: with BOOST_DISABLE_THREADS and page_allocator_by_size, churn drops from 58.0 to 9.4 ns (6.2x), cycles from 139.6 to 50.9 ns per node (2.7x) and bulk release from 133.4 to 74.8 ns (1.8x): the global root_ptr mutex, atomic counts and the pool mutex go away. shared_node_ptr churn drops 2.2x (61.8 to 28.7 ns), unique_ptr 3.2x (18.6 to 5.8 ns, the page pool's mutex); std::allocator itself does not change (16.3 vs. 15.8 ns).
- Striped locks (BOOST_ROOT_PTR_STRIPED_LOCKS) trade single-threaded speed for scaling. With 4 threads, root_ptr goes from 313 to 29.7 ns per object with std::allocator (10.5x), and from 309 to 30.2 ns with page_allocator_by_size plus thread caches (10.2x); without the caches the page pool's mutex caps the gain at 1.36x. Single-threaded every root_ptr scenario is slower: 1.5x to 2.6x (geometric means), e.g. churn 58.0 to 120.2 ns and cycles 139.6 to 451.1 ns. shared_node_ptr takes no root_ptr lock and does not change (0.99-1.04x).
- Per-thread caches (BOOST_PAGE_ALLOCATOR_THREAD_CACHE) take the page pool's mutex off the common path. With 4 threads and page_allocator_by_size: unique_ptr 182.9 to 3.4 ns, shared_ptr 166.5 to 4.3 ns, shared_node_ptr 238.9 to 12.4 ns - now level with or faster than std::allocator (5.1, 5.8, 15.2 ns); root_ptr needs striped locks as well (above). Churn: the allocator alone 3.1x faster (17.1 to 5.5 ns), unique_ptr 2.9x, shared_ptr 2.7x, shared_node_ptr 1.6x, root_ptr 1.3x. Bulk, mixed, types, cycles and resident memory do not change (0.99-1.04x).
- Zeroization (BOOST_ZEROIZATION) costs nothing measurable: the geometric mean of time without / with it is 0.97-1.01x for every pointer and scenario, and single cells scatter within run-to-run noise (0.72-1.22x). Clearing a 72-248 byte node is small next to a 50-150 ns release. (Checked separately that the macro is active: a released node reads back as zeros.)
- root_ptr against the standard pointers (page_allocator_by_size): churn costs 3.1x unique_ptr and 2.6x shared_ptr with thread support (58.0 vs. 18.6 and 22.7 ns), 1.6x and 1.0x without (9.4 vs. 5.8 and 9.3 ns). It holds 100,000 objects in 12.5 MB against 7.0 MB (unique_ptr) and 9.3 MB (shared_ptr). It is the only one of them that reclaims cycles.
- shared_node_ptr (what FCXXSS_SHARED_PTR emits) against root_ptr, both on page_allocator_by_size: single-threaded with thread support it costs 1.06-1.15x as much (churn 61.8 vs. 58.0 ns) and holds the same objects in 19-25% more memory (a control block per object on top of the node). With 4 threads it has no global lock: 1.6x faster (239 vs. 389 ns), 24x with std::allocator and 25x with the thread caches (12.4 vs. 309 ns). Without thread support it is the slower one: churn 3.1x (28.7 vs. 9.4 ns). A cycle through it is never released.
- page_allocator_by_type costs more with many sparse types: 32 types of 100 objects each take 2.1x to 4.3x the time of page_allocator_by_size and 1.6-1.9 MB more memory (3.8-4.2 MB against 1.9-2.3 MB), because each type fills a 64 KiB page of its own.
- Noise: the slowest of a cell's 5 runs is 1.21x its fastest at the median (1.36x at the 90th percentile, up to 4x in the 4-thread scenario), so the medians are reported, and differences below about 10% are not significant. The run took 2.5 hours on a shared desktop machine.

## Setup

|  |  |
|---|---|
| Measured | 2026-09-29 21:32 to 2026-09-30 00:02 |
| CPU | Intel(R) Core(TM) i7-4700HQ CPU @ 2.40GHz, 8 logical CPUs |
| OS | Linux 5.15.0-194-generic |
| Compiler | clang version 23.0.0git, glibc 2.35 |
| Flags | `-std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include` |
| Load | load average 15.14 13.21 9.03 at the start, 1.19 1.54 1.72 at the end |
| Method | median of 5 runs; every measurement in a fresh process, after the CPU has cooled (this machine throttles under the four-thread load); loops ordered scenario > allocator > build > pointer so drift hits every build alike |

### Builds

The macros that change what the benchmark runs. Striped locks and thread caches are inactive without threads, hence 8 builds with threads and 2 without. `BOOST_NO_EXCEPTIONS` only changes `root_ptr`'s static and dynamic cast constructors, which no scenario uses.

| build | `BOOST_DISABLE_THREADS` | `BOOST_ROOT_PTR_STRIPED_LOCKS` | `BOOST_PAGE_ALLOCATOR_THREAD_CACHE` | `BOOST_ZEROIZATION` | pointers measured |
|---|---|---|---|---|---|
| threads (default) |  |  |  |  | all |
| threads + striped |  | yes |  |  | `root_ptr`, `shared_node_ptr` |
| threads + caches |  |  | yes |  | all |
| threads + striped + caches |  | yes | yes |  | `root_ptr`, `shared_node_ptr` |
| threads + zero |  |  |  | yes | `root_ptr`, `shared_node_ptr` |
| threads + striped + zero |  | yes |  | yes | `root_ptr`, `shared_node_ptr` |
| threads + caches + zero |  |  | yes | yes | `root_ptr`, `shared_node_ptr` |
| threads + striped + caches + zero |  | yes | yes | yes | `root_ptr`, `shared_node_ptr` |
| no threads | yes |  |  |  | all |
| no threads + zero | yes |  |  | yes | `root_ptr`, `shared_node_ptr` |

| macro | what changes |
|---|---|
| `BOOST_DISABLE_THREADS` | no `root_ptr` lock; plain reference counts; Boost pools use `null_mutex`; page pools a no-op mutex. `std::allocator` (glibc) and `std::shared_ptr`'s counts (libstdc++) do not change |
| `BOOST_ROOT_PTR_STRIPED_LOCKS` | `root_ptr`'s global mutex replaced by 1024 spin locks indexed by root address |
| `BOOST_PAGE_ALLOCATOR_THREAD_CACHE` | each thread keeps up to 16 KiB of free blocks of every page pool, moving half at once to or from the pool under its mutex; only the page allocators change |
| `BOOST_ZEROIZATION` | every node is cleared (`memset`) before its memory is released |

`std::unique_ptr`, `std::shared_ptr` and the allocator alone use no node and no `root_ptr` lock, so striped locks and zeroization do not change their code: they are measured in the three builds where it can differ (threads, threads + caches, no threads), and the tables show those values in *italics* for the equivalent builds.

### Allocators, pointers, scenarios

| allocator |  |
|---|---|
| pool | `boost::pool_allocator` |
| fast | `boost::fast_pool_allocator` |
| std | `std::allocator` |
| page_type | `boost::page_allocator_by_type` |
| page_size | `boost::page_allocator_by_size`, the default |

| pointer | construction |
|---|---|
| `boost::root_ptr` | `root_ptr<T>(x, new node<T, A<T>>(...))` |
| `boost::shared_node_ptr` | `shared_node_ptr<T>(x, new node<T, A<T>>(...))` - what `FCXXSS_SHARED_PTR` emits: a `std::shared_ptr` owning the same node |
| `std::unique_ptr` | `allocate_unique<T>(A<T>(), ...)`, a helper whose deleter frees through the same allocator |
| `std::shared_ptr` | `std::allocate_shared<T>(A<T>(), ...)` |
| allocator alone | one node-sized block (72 B) from the allocator, no pointer |

| scenario | one operation |
|---|---|
| raw_churn | allocate and free one 72-byte block, 4,000,000 times |
| raw_batch | allocate 100,000 blocks, then free them in order |
| churn | create and drop one 48-byte object, 1,000,000 times |
| bulk | 100,000 live 48-byte objects, released together |
| mixed | 50,000 each of 16, 48 and 200 bytes, interleaved, released together |
| cycles | 25,000 two-node cycles, reclaimed with their proxy (root_ptr only) |
| threads | 4 threads, each creating and dropping 250,000 objects (wall clock / all operations) |
| types | 32 types of equal size, 100 live objects of each |

## Time per operation

ns per operation, median (lower is better). Rows: builds; columns: allocators. *Italics*: the value of the equivalent build (see Builds).

### churn

create and drop one 48-byte object, 1,000,000 times.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 79.1 | 57.6 | 58.8 | 58.4 | 58.0 |
| threads + striped | 142 | 127 | 120 | 117 | 120 |
| threads + caches | 81.7 | 57.7 | 59.8 | 44.5 | 44.8 |
| threads + striped + caches | 128 | 114 | 119 | 111 | 106 |
| threads + zero | 77.1 | 63.7 | 54.1 | 64.9 | 59.3 |
| threads + striped + zero | 140 | 114 | 113 | 116 | 117 |
| threads + caches + zero | 79.0 | 59.6 | 53.2 | 44.6 | 47.3 |
| threads + striped + caches + zero | 130 | 117 | 115 | 105 | 106 |
| no threads | 36.2 | 10.6 | 25.6 | 9.37 | 9.39 |
| no threads + zero | 40.7 | 14.7 | 21.9 | 11.8 | 10.6 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 84.6 | 46.7 | 46.2 | 51.3 | 61.8 |
| threads + striped | 72.7 | 48.1 | 54.2 | 50.4 | 50.7 |
| threads + caches | 70.1 | 47.5 | 51.4 | 38.9 | 38.6 |
| threads + striped + caches | 67.5 | 47.9 | 45.7 | 38.5 | 38.1 |
| threads + zero | 73.2 | 48.3 | 49.3 | 50.9 | 50.7 |
| threads + striped + zero | 68.8 | 46.7 | 46.1 | 51.8 | 50.9 |
| threads + caches + zero | 71.2 | 46.7 | 46.0 | 37.3 | 39.3 |
| threads + striped + caches + zero | 74.7 | 47.4 | 49.8 | 38.8 | 39.4 |
| no threads | 54.2 | 27.9 | 46.1 | 28.7 | 28.7 |
| no threads + zero | 54.3 | 35.4 | 46.6 | 29.2 | 31.9 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 46.2 | 26.3 | 16.3 | 17.1 | 18.6 |
| threads + striped | *46.2* | *26.3* | *16.3* | *17.1* | *18.6* |
| threads + caches | 47.9 | 27.8 | 18.5 | 7.72 | 6.41 |
| threads + striped + caches | *47.9* | *27.8* | *18.5* | *7.72* | *6.41* |
| threads + zero | *46.2* | *26.3* | *16.3* | *17.1* | *18.6* |
| threads + striped + zero | *46.2* | *26.3* | *16.3* | *17.1* | *18.6* |
| threads + caches + zero | *47.9* | *27.8* | *18.5* | *7.72* | *6.41* |
| threads + striped + caches + zero | *47.9* | *27.8* | *18.5* | *7.72* | *6.41* |
| no threads | 27.3 | 4.05 | 15.8 | 5.99 | 5.83 |
| no threads + zero | *27.3* | *4.05* | *15.8* | *5.99* | *5.83* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 63.4 | 43.5 | 17.6 | 20.1 | 22.7 |
| threads + striped | *63.4* | *43.5* | *17.6* | *20.1* | *22.7* |
| threads + caches | 70.9 | 48.4 | 21.8 | 10.1 | 8.48 |
| threads + striped + caches | *70.9* | *48.4* | *21.8* | *10.1* | *8.48* |
| threads + zero | *63.4* | *43.5* | *17.6* | *20.1* | *22.7* |
| threads + striped + zero | *63.4* | *43.5* | *17.6* | *20.1* | *22.7* |
| threads + caches + zero | *70.9* | *48.4* | *21.8* | *10.1* | *8.48* |
| threads + striped + caches + zero | *70.9* | *48.4* | *21.8* | *10.1* | *8.48* |
| no threads | 30.0 | 5.80 | 19.1 | 10.4 | 9.33 |
| no threads + zero | *30.0* | *5.80* | *19.1* | *10.4* | *9.33* |

### bulk

100,000 live 48-byte objects, released together.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 141,871 | 138 | 144 | 131 | 133 |
| threads + striped | 141,697 | 240 | 252 | 235 | 241 |
| threads + caches | 140,769 | 137 | 138 | 131 | 131 |
| threads + striped + caches | 143,332 | 249 | 250 | 240 | 233 |
| threads + zero | 140,945 | 138 | 144 | 131 | 129 |
| threads + striped + zero | 142,621 | 244 | 255 | 234 | 232 |
| threads + caches + zero | 143,739 | 134 | 144 | 137 | 135 |
| threads + striped + caches + zero | 143,132 | 246 | 250 | 241 | 236 |
| no threads | 145,369 | 81.2 | 91.8 | 74.8 | 74.8 |
| no threads + zero | 145,919 | 84.8 | 89.5 | 76.9 | 76.4 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 144,252 | 151 | 150 | 147 | 142 |
| threads + striped | 142,903 | 150 | 151 | 142 | 143 |
| threads + caches | 142,988 | 151 | 156 | 150 | 145 |
| threads + striped + caches | 141,926 | 150 | 149 | 147 | 148 |
| threads + zero | 143,772 | 155 | 155 | 142 | 142 |
| threads + striped + zero | 143,751 | 151 | 155 | 142 | 143 |
| threads + caches + zero | 145,759 | 149 | 154 | 148 | 150 |
| threads + striped + caches + zero | 144,356 | 151 | 150 | 150 | 145 |
| no threads | 142,011 | 125 | 141 | 116 | 116 |
| no threads + zero | 144,348 | 123 | 137 | 119 | 114 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 85,265 | 63.0 | 65.9 | 47.5 | 47.6 |
| threads + striped | *85,265* | *63.0* | *65.9* | *47.5* | *47.6* |
| threads + caches | 84,610 | 62.2 | 65.8 | 44.2 | 42.9 |
| threads + striped + caches | *84,610* | *62.2* | *65.8* | *44.2* | *42.9* |
| threads + zero | *85,265* | *63.0* | *65.9* | *47.5* | *47.6* |
| threads + striped + zero | *85,265* | *63.0* | *65.9* | *47.5* | *47.6* |
| threads + caches + zero | *84,610* | *62.2* | *65.8* | *44.2* | *42.9* |
| threads + striped + caches + zero | *84,610* | *62.2* | *65.8* | *44.2* | *42.9* |
| no threads | 88,068 | 42.3 | 64.8 | 41.4 | 40.9 |
| no threads + zero | *88,068* | *42.3* | *64.8* | *41.4* | *40.9* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 136,876 | 99.7 | 77.2 | 64.2 | 63.5 |
| threads + striped | *136,876* | *99.7* | *77.2* | *64.2* | *63.5* |
| threads + caches | 138,142 | 100 | 79.4 | 61.2 | 61.3 |
| threads + striped + caches | *138,142* | *100* | *79.4* | *61.2* | *61.3* |
| threads + zero | *136,876* | *99.7* | *77.2* | *64.2* | *63.5* |
| threads + striped + zero | *136,876* | *99.7* | *77.2* | *64.2* | *63.5* |
| threads + caches + zero | *138,142* | *100* | *79.4* | *61.2* | *61.3* |
| threads + striped + caches + zero | *138,142* | *100* | *79.4* | *61.2* | *61.3* |
| no threads | 139,020 | 58.8 | 80.0 | 55.9 | 55.3 |
| no threads + zero | *139,020* | *58.8* | *80.0* | *55.9* | *55.3* |

### mixed

50,000 each of 16, 48 and 200 bytes, interleaved, released together.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 99,512 | 170 | 178 | 137 | 155 |
| threads + striped | 97,170 | 267 | 277 | 243 | 258 |
| threads + caches | 95,371 | 162 | 182 | 144 | 142 |
| threads + striped + caches | 96,602 | 252 | 281 | 254 | 242 |
| threads + zero | 98,230 | 166 | 154 | 144 | 144 |
| threads + striped + zero | 103,182 | 256 | 272 | 239 | 245 |
| threads + caches + zero | 98,105 | 160 | 180 | 156 | 147 |
| threads + striped + caches + zero | 95,402 | 256 | 279 | 250 | 265 |
| no threads | 101,238 | 112 | 129 | 98.9 | 87.1 |
| no threads + zero | 100,616 | 117 | 139 | 103 | 104 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 97,293 | 186 | 193 | 155 | 176 |
| threads + striped | 98,320 | 184 | 229 | 154 | 156 |
| threads + caches | 98,280 | 180 | 209 | 163 | 155 |
| threads + striped + caches | 97,900 | 159 | 214 | 164 | 160 |
| threads + zero | 99,161 | 180 | 199 | 173 | 165 |
| threads + striped + zero | 97,605 | 159 | 201 | 149 | 162 |
| threads + caches + zero | 99,258 | 175 | 209 | 166 | 170 |
| threads + striped + caches + zero | 97,778 | 156 | 182 | 157 | 156 |
| no threads | 105,652 | 156 | 199 | 145 | 126 |
| no threads + zero | 99,523 | 155 | 199 | 148 | 146 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 83,971 | 91.5 | 93.8 | 63.0 | 72.6 |
| threads + striped | *83,971* | *91.5* | *93.8* | *63.0* | *72.6* |
| threads + caches | 81,330 | 84.2 | 91.3 | 70.7 | 70.0 |
| threads + striped + caches | *81,330* | *84.2* | *91.3* | *70.7* | *70.0* |
| threads + zero | *83,971* | *91.5* | *93.8* | *63.0* | *72.6* |
| threads + striped + zero | *83,971* | *91.5* | *93.8* | *63.0* | *72.6* |
| threads + caches + zero | *81,330* | *84.2* | *91.3* | *70.7* | *70.0* |
| threads + striped + caches + zero | *81,330* | *84.2* | *91.3* | *70.7* | *70.0* |
| no threads | 82,876 | 70.8 | 93.4 | 62.4 | 66.8 |
| no threads + zero | *82,876* | *70.8* | *93.4* | *62.4* | *66.8* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 96,200 | 127 | 122 | 89.7 | 91.2 |
| threads + striped | *96,200* | *127* | *122* | *89.7* | *91.2* |
| threads + caches | 95,502 | 121 | 104 | 90.2 | 90.5 |
| threads + striped + caches | *95,502* | *121* | *104* | *90.2* | *90.5* |
| threads + zero | *96,200* | *127* | *122* | *89.7* | *91.2* |
| threads + striped + zero | *96,200* | *127* | *122* | *89.7* | *91.2* |
| threads + caches + zero | *95,502* | *121* | *104* | *90.2* | *90.5* |
| threads + striped + caches + zero | *95,502* | *121* | *104* | *90.2* | *90.5* |
| no threads | 96,604 | 88.6 | 116 | 82.7 | 82.7 |
| no threads + zero | *96,604* | *88.6* | *116* | *82.7* | *82.7* |

### cycles

25,000 two-node cycles, reclaimed with their proxy (root_ptr only).

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 68,865 | 155 | 165 | 144 | 140 |
| threads + striped | 69,241 | 488 | 492 | 463 | 451 |
| threads + caches | 68,397 | 153 | 165 | 137 | 136 |
| threads + striped + caches | 68,619 | 479 | 501 | 455 | 463 |
| threads + zero | 68,972 | 139 | 165 | 143 | 148 |
| threads + striped + zero | 69,952 | 486 | 491 | 452 | 470 |
| threads + caches + zero | 69,343 | 138 | 162 | 136 | 136 |
| threads + striped + caches + zero | 70,027 | 421 | 476 | 475 | 469 |
| no threads | 68,683 | 66.2 | 63.6 | 50.7 | 50.9 |
| no threads + zero | 69,447 | 65.7 | 78.1 | 52.1 | 52.4 |

### threads

4 threads, each creating and dropping 250,000 objects (wall clock / all operations).

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 462 | 393 | 313 | 402 | 389 |
| threads + striped | 324 | 284 | 29.7 | 285 | 286 |
| threads + caches | 482 | 385 | 289 | 280 | 309 |
| threads + striped + caches | 320 | 290 | 34.4 | 28.0 | 30.2 |
| threads + zero | 475 | 383 | 279 | 418 | 405 |
| threads + striped + zero | 325 | 288 | 33.7 | 295 | 299 |
| threads + caches + zero | 469 | 400 | 322 | 306 | 276 |
| threads + striped + caches + zero | 316 | 293 | 34.1 | 33.1 | 31.8 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 283 | 216 | 12.8 | 233 | 239 |
| threads + striped | 268 | 207 | 15.5 | 217 | 230 |
| threads + caches | 270 | 216 | 15.2 | 12.9 | 12.4 |
| threads + striped + caches | 254 | 205 | 15.6 | 11.1 | 12.4 |
| threads + zero | 282 | 241 | 16.1 | 223 | 218 |
| threads + striped + zero | 274 | 216 | 15.6 | 230 | 215 |
| threads + caches + zero | 264 | 219 | 12.6 | 12.8 | 13.3 |
| threads + striped + caches + zero | 263 | 214 | 15.1 | 12.5 | 10.3 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 273 | 224 | 5.05 | 169 | 183 |
| threads + striped | *273* | *224* | *5.05* | *169* | *183* |
| threads + caches | 263 | 222 | 5.09 | 1.95 | 3.44 |
| threads + striped + caches | *263* | *222* | *5.09* | *1.95* | *3.44* |
| threads + zero | *273* | *224* | *5.05* | *169* | *183* |
| threads + striped + zero | *273* | *224* | *5.05* | *169* | *183* |
| threads + caches + zero | *263* | *222* | *5.09* | *1.95* | *3.44* |
| threads + striped + caches + zero | *263* | *222* | *5.09* | *1.95* | *3.44* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 376 | 307 | 6.00 | 151 | 166 |
| threads + striped | *376* | *307* | *6.00* | *151* | *166* |
| threads + caches | 379 | 295 | 5.77 | 2.68 | 4.35 |
| threads + striped + caches | *379* | *295* | *5.77* | *2.68* | *4.35* |
| threads + zero | *376* | *307* | *6.00* | *151* | *166* |
| threads + striped + zero | *376* | *307* | *6.00* | *151* | *166* |
| threads + caches + zero | *379* | *295* | *5.77* | *2.68* | *4.35* |
| threads + striped + caches + zero | *379* | *295* | *5.77* | *2.68* | *4.35* |

### types

32 types of equal size, 100 live objects of each.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 239 | 141 | 160 | 371 | 146 |
| threads + striped | 339 | 281 | 273 | 443 | 269 |
| threads + caches | 249 | 141 | 159 | 327 | 143 |
| threads + striped + caches | 343 | 251 | 278 | 504 | 262 |
| threads + zero | 257 | 166 | 160 | 354 | 152 |
| threads + striped + zero | 350 | 284 | 273 | 437 | 265 |
| threads + caches + zero | 244 | 161 | 160 | 378 | 145 |
| threads + striped + caches + zero | 338 | 288 | 281 | 503 | 272 |
| no threads | 191 | 104 | 105 | 317 | 83.2 |
| no threads + zero | 215 | 101 | 92.6 | 322 | 90.2 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 249 | 168 | 169 | 346 | 168 |
| threads + striped | 263 | 170 | 173 | 344 | 168 |
| threads + caches | 256 | 148 | 176 | 355 | 168 |
| threads + striped + caches | 252 | 159 | 175 | 388 | 154 |
| threads + zero | 255 | 171 | 176 | 352 | 164 |
| threads + striped + zero | 251 | 167 | 174 | 389 | 162 |
| threads + caches + zero | 255 | 173 | 169 | 385 | 165 |
| threads + striped + caches + zero | 267 | 170 | 175 | 396 | 158 |
| no threads | 244 | 150 | 156 | 363 | 138 |
| no threads + zero | 243 | 151 | 142 | 366 | 138 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 170 | 74.5 | 75.1 | 283 | 66.0 |
| threads + striped | *170* | *74.5* | *75.1* | *283* | *66.0* |
| threads + caches | 177 | 73.4 | 76.6 | 287 | 63.2 |
| threads + striped + caches | *177* | *73.4* | *76.6* | *287* | *63.2* |
| threads + zero | *170* | *74.5* | *75.1* | *283* | *66.0* |
| threads + striped + zero | *170* | *74.5* | *75.1* | *283* | *66.0* |
| threads + caches + zero | *177* | *73.4* | *76.6* | *287* | *63.2* |
| threads + striped + caches + zero | *177* | *73.4* | *76.6* | *287* | *63.2* |
| no threads | 162 | 55.3 | 72.9 | 297 | 57.7 |
| no threads + zero | *162* | *55.3* | *72.9* | *297* | *57.7* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 204 | 112 | 88.2 | 276 | 85.8 |
| threads + striped | *204* | *112* | *88.2* | *276* | *85.8* |
| threads + caches | 208 | 97.6 | 89.8 | 310 | 76.2 |
| threads + striped + caches | *208* | *97.6* | *89.8* | *310* | *76.2* |
| threads + zero | *204* | *112* | *88.2* | *276* | *85.8* |
| threads + striped + zero | *204* | *112* | *88.2* | *276* | *85.8* |
| threads + caches + zero | *208* | *97.6* | *89.8* | *310* | *76.2* |
| threads + striped + caches + zero | *208* | *97.6* | *89.8* | *310* | *76.2* |
| no threads | 172 | 68.8 | 88.9 | 306 | 74.7 |
| no threads + zero | *172* | *68.8* | *88.9* | *306* | *74.7* |

### allocator alone

raw_churn - allocate and free one 72-byte block, 4,000,000 times:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 42.7 | 17.6 | 17.2 | 16.8 | 17.1 |
| threads + striped | *42.7* | *17.6* | *17.2* | *16.8* | *17.1* |
| threads + caches | 41.4 | 16.7 | 15.9 | 5.33 | 5.52 |
| threads + striped + caches | *41.4* | *16.7* | *15.9* | *5.33* | *5.52* |
| threads + zero | *42.7* | *17.6* | *17.2* | *16.8* | *17.1* |
| threads + striped + zero | *42.7* | *17.6* | *17.2* | *16.8* | *17.1* |
| threads + caches + zero | *41.4* | *16.7* | *15.9* | *5.33* | *5.52* |
| threads + striped + caches + zero | *41.4* | *16.7* | *15.9* | *5.33* | *5.52* |
| no threads | 27.2 | 3.33 | 15.8 | 4.01 | 4.77 |
| no threads + zero | *27.2* | *3.33* | *15.8* | *4.01* | *4.77* |

raw_batch - allocate 100,000 blocks, then free them in order:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 70,717 | 30.8 | 33.8 | 28.9 | 28.9 |
| threads + striped | *70,717* | *30.8* | *33.8* | *28.9* | *28.9* |
| threads + caches | 71,325 | 30.4 | 33.5 | 28.3 | 27.6 |
| threads + striped + caches | *71,325* | *30.4* | *33.5* | *28.3* | *27.6* |
| threads + zero | *70,717* | *30.8* | *33.8* | *28.9* | *28.9* |
| threads + striped + zero | *70,717* | *30.8* | *33.8* | *28.9* | *28.9* |
| threads + caches + zero | *71,325* | *30.4* | *33.5* | *28.3* | *27.6* |
| threads + striped + caches + zero | *71,325* | *30.4* | *33.5* | *28.3* | *27.6* |
| no threads | 71,570 | 26.6 | 33.8 | 24.9 | 25.1 |
| no threads + zero | *71,570* | *26.6* | *33.8* | *24.9* | *25.1* |

## Resident memory

Resident memory grown while the objects are live, MB, median (lower is better).

### bulk

100,000 live 48-byte objects, released together.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 13.7 | 13.7 | 12.4 | 12.4 | 12.5 |
| threads + striped | 13.7 | 13.8 | 12.5 | 12.5 | 12.5 |
| threads + caches | 13.7 | 13.7 | 12.5 | 12.5 | 12.5 |
| threads + striped + caches | 13.7 | 13.7 | 12.4 | 12.5 | 12.4 |
| threads + zero | 13.7 | 13.8 | 12.5 | 12.4 | 12.4 |
| threads + striped + zero | 13.7 | 13.7 | 12.5 | 12.4 | 12.4 |
| threads + caches + zero | 13.7 | 13.8 | 12.4 | 12.4 | 12.4 |
| threads + striped + caches + zero | 13.7 | 13.7 | 12.5 | 12.5 | 12.5 |
| no threads | 13.6 | 13.8 | 12.3 | 12.4 | 12.4 |
| no threads + zero | 13.6 | 13.7 | 12.4 | 12.4 | 12.4 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 16.9 | 16.9 | 15.3 | 15.5 | 15.6 |
| threads + striped | 16.8 | 16.9 | 15.3 | 15.6 | 15.6 |
| threads + caches | 16.8 | 16.9 | 15.3 | 15.6 | 15.6 |
| threads + striped + caches | 16.8 | 16.9 | 15.6 | 15.6 | 15.6 |
| threads + zero | 16.8 | 16.9 | 15.3 | 15.6 | 15.6 |
| threads + striped + zero | 16.8 | 16.8 | 15.6 | 15.6 | 15.5 |
| threads + caches + zero | 16.8 | 16.8 | 15.3 | 15.6 | 15.6 |
| threads + striped + caches + zero | 16.8 | 16.9 | 15.5 | 15.6 | 15.6 |
| no threads | 16.8 | 16.8 | 15.3 | 15.6 | 15.6 |
| no threads + zero | 16.8 | 16.9 | 15.3 | 15.6 | 15.5 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 8.3 | 8.7 | 8.5 | 7.0 | 7.0 |
| threads + striped | *8.3* | *8.7* | *8.5* | *7.0* | *7.0* |
| threads + caches | 8.3 | 8.4 | 8.5 | 7.0 | 7.0 |
| threads + striped + caches | *8.3* | *8.4* | *8.5* | *7.0* | *7.0* |
| threads + zero | *8.3* | *8.7* | *8.5* | *7.0* | *7.0* |
| threads + striped + zero | *8.3* | *8.7* | *8.5* | *7.0* | *7.0* |
| threads + caches + zero | *8.3* | *8.4* | *8.5* | *7.0* | *7.0* |
| threads + striped + caches + zero | *8.3* | *8.4* | *8.5* | *7.0* | *7.0* |
| no threads | 8.3 | 8.3 | 8.5 | 7.0 | 7.0 |
| no threads + zero | *8.3* | *8.3* | *8.5* | *7.0* | *7.0* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 11.2 | 11.2 | 10.8 | 9.2 | 9.3 |
| threads + striped | *11.2* | *11.2* | *10.8* | *9.2* | *9.3* |
| threads + caches | 11.1 | 11.2 | 10.8 | 9.4 | 9.3 |
| threads + striped + caches | *11.1* | *11.2* | *10.8* | *9.4* | *9.3* |
| threads + zero | *11.2* | *11.2* | *10.8* | *9.2* | *9.3* |
| threads + striped + zero | *11.2* | *11.2* | *10.8* | *9.2* | *9.3* |
| threads + caches + zero | *11.1* | *11.2* | *10.8* | *9.4* | *9.3* |
| threads + striped + caches + zero | *11.1* | *11.2* | *10.8* | *9.4* | *9.3* |
| no threads | 11.1 | 11.1 | 10.8 | 9.3 | 9.3 |
| no threads + zero | *11.1* | *11.1* | *10.8* | *9.3* | *9.3* |

### mixed

50,000 each of 16, 48 and 200 bytes, interleaved, released together.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 27.4 | 27.4 | 23.8 | 23.3 | 23.3 |
| threads + striped | 27.4 | 27.4 | 23.9 | 23.3 | 23.3 |
| threads + caches | 27.3 | 27.4 | 23.8 | 23.3 | 23.3 |
| threads + striped + caches | 27.4 | 27.4 | 23.8 | 23.4 | 23.3 |
| threads + zero | 27.3 | 27.4 | 23.8 | 23.2 | 23.3 |
| threads + striped + zero | 27.4 | 27.4 | 23.8 | 23.4 | 23.4 |
| threads + caches + zero | 27.3 | 27.5 | 23.8 | 23.3 | 23.3 |
| threads + striped + caches + zero | 27.4 | 27.4 | 23.9 | 23.3 | 23.3 |
| no threads | 27.3 | 27.4 | 23.7 | 23.2 | 23.2 |
| no threads + zero | 27.3 | 27.3 | 23.7 | 23.2 | 23.3 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 31.8 | 31.9 | 28.5 | 27.7 | 27.7 |
| threads + striped | 31.8 | 31.8 | 28.5 | 27.8 | 27.7 |
| threads + caches | 31.8 | 31.8 | 28.4 | 27.7 | 27.6 |
| threads + striped + caches | 31.8 | 31.8 | 28.4 | 27.7 | 27.7 |
| threads + zero | 31.8 | 31.8 | 28.5 | 27.8 | 27.7 |
| threads + striped + zero | 31.8 | 31.9 | 28.5 | 27.7 | 27.7 |
| threads + caches + zero | 31.8 | 31.8 | 28.5 | 27.8 | 27.7 |
| threads + striped + caches + zero | 31.8 | 31.9 | 28.4 | 27.7 | 27.7 |
| no threads | 31.8 | 31.8 | 28.4 | 27.7 | 27.7 |
| no threads + zero | 31.8 | 31.8 | 28.4 | 27.7 | 27.6 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 19.4 | 19.5 | 17.3 | 15.7 | 15.9 |
| threads + striped | *19.4* | *19.5* | *17.3* | *15.7* | *15.9* |
| threads + caches | 19.4 | 19.4 | 17.2 | 15.8 | 15.9 |
| threads + striped + caches | *19.4* | *19.4* | *17.2* | *15.8* | *15.9* |
| threads + zero | *19.4* | *19.5* | *17.3* | *15.7* | *15.9* |
| threads + striped + zero | *19.4* | *19.5* | *17.3* | *15.7* | *15.9* |
| threads + caches + zero | *19.4* | *19.4* | *17.2* | *15.8* | *15.9* |
| threads + striped + caches + zero | *19.4* | *19.4* | *17.2* | *15.8* | *15.9* |
| no threads | 19.3 | 19.3 | 17.2 | 15.7 | 15.8 |
| no threads + zero | *19.3* | *19.3* | *17.2* | *15.7* | *15.8* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 23.5 | 23.7 | 20.6 | 19.3 | 19.5 |
| threads + striped | *23.5* | *23.7* | *20.6* | *19.3* | *19.5* |
| threads + caches | 23.5 | 23.6 | 20.6 | 19.4 | 19.5 |
| threads + striped + caches | *23.5* | *23.6* | *20.6* | *19.4* | *19.5* |
| threads + zero | *23.5* | *23.7* | *20.6* | *19.3* | *19.5* |
| threads + striped + zero | *23.5* | *23.7* | *20.6* | *19.3* | *19.5* |
| threads + caches + zero | *23.5* | *23.6* | *20.6* | *19.4* | *19.5* |
| threads + striped + caches + zero | *23.5* | *23.6* | *20.6* | *19.4* | *19.5* |
| no threads | 23.5 | 23.5 | 20.6 | 19.4 | 19.3 |
| no threads + zero | *23.5* | *23.5* | *20.6* | *19.4* | *19.3* |

### types

32 types of equal size, 100 live objects of each.

`boost::root_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 2.1 | 2.2 | 2.3 | 3.8 | 2.2 |
| threads + striped | 2.3 | 2.2 | 2.3 | 4.1 | 2.3 |
| threads + caches | 2.1 | 2.2 | 2.3 | 3.8 | 2.3 |
| threads + striped + caches | 2.2 | 2.2 | 2.3 | 4.1 | 2.3 |
| threads + zero | 2.2 | 2.2 | 2.3 | 3.8 | 2.2 |
| threads + striped + zero | 2.1 | 2.2 | 2.2 | 4.1 | 2.3 |
| threads + caches + zero | 2.1 | 2.2 | 2.2 | 3.9 | 2.3 |
| threads + striped + caches + zero | 2.2 | 2.3 | 2.3 | 4.2 | 2.3 |
| no threads | 2.1 | 2.2 | 2.2 | 3.7 | 2.3 |
| no threads + zero | 2.1 | 2.2 | 2.2 | 3.8 | 2.3 |

`boost::shared_node_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 2.2 | 2.3 | 2.2 | 4.1 | 2.2 |
| threads + striped | 2.2 | 2.2 | 2.2 | 4.1 | 2.2 |
| threads + caches | 2.3 | 2.3 | 2.2 | 4.1 | 2.1 |
| threads + striped + caches | 2.2 | 2.2 | 2.2 | 4.1 | 2.2 |
| threads + zero | 2.2 | 2.2 | 2.2 | 4.1 | 2.2 |
| threads + striped + zero | 2.2 | 2.3 | 2.2 | 4.1 | 2.2 |
| threads + caches + zero | 2.2 | 2.2 | 2.2 | 4.1 | 2.2 |
| threads + striped + caches + zero | 2.2 | 2.2 | 2.1 | 4.1 | 2.2 |
| no threads | 2.1 | 2.2 | 2.2 | 4.0 | 2.2 |
| no threads + zero | 2.2 | 2.2 | 2.2 | 4.0 | 2.2 |

`std::unique_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 1.9 | 1.9 | 1.9 | 3.7 | 1.9 |
| threads + striped | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + caches | 1.9 | 1.9 | 1.9 | 3.7 | 1.9 |
| threads + striped + caches | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + zero | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + striped + zero | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + caches + zero | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + striped + caches + zero | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| no threads | 1.8 | 1.9 | 1.9 | 3.7 | 1.9 |
| no threads + zero | *1.8* | *1.9* | *1.9* | *3.7* | *1.9* |

`std::shared_ptr`:

| build | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| threads (default) | 1.9 | 1.9 | 1.9 | 3.7 | 1.9 |
| threads + striped | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + caches | 1.9 | 1.9 | 1.9 | 3.8 | 2.0 |
| threads + striped + caches | *1.9* | *1.9* | *1.9* | *3.8* | *2.0* |
| threads + zero | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + striped + zero | *1.9* | *1.9* | *1.9* | *3.7* | *1.9* |
| threads + caches + zero | *1.9* | *1.9* | *1.9* | *3.8* | *2.0* |
| threads + striped + caches + zero | *1.9* | *1.9* | *1.9* | *3.8* | *2.0* |
| no threads | 1.9 | 1.9 | 1.8 | 3.7 | 1.9 |
| no threads + zero | *1.9* | *1.9* | *1.8* | *3.7* | *1.9* |

## Effect of each setting

Time without the setting / time with it, for each pair of builds that differ by that setting alone (higher is better: above 1.00x the setting is faster). *same*: the pointer's code does not change with it.

### Turning thread support off (BOOST_DISABLE_THREADS)

default: threads (default) -> no threads

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `allocator alone` raw_churn | 1.57x | 5.28x | 1.09x | 4.18x | 3.59x |
| `allocator alone` raw_batch | 0.99x | 1.16x | 1.00x | 1.16x | 1.15x |
| `boost::root_ptr` churn | 2.19x | 5.43x | 2.29x | 6.23x | 6.17x |
| `boost::root_ptr` bulk | 0.98x | 1.70x | 1.57x | 1.76x | 1.78x |
| `boost::root_ptr` mixed | 0.98x | 1.52x | 1.37x | 1.38x | 1.77x |
| `boost::root_ptr` cycles | 1.00x | 2.35x | 2.59x | 2.84x | 2.74x |
| `boost::root_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `boost::root_ptr` types | 1.25x | 1.36x | 1.52x | 1.17x | 1.76x |
| `boost::shared_node_ptr` churn | 1.56x | 1.67x | 1.00x | 1.79x | 2.16x |
| `boost::shared_node_ptr` bulk | 1.02x | 1.21x | 1.07x | 1.26x | 1.22x |
| `boost::shared_node_ptr` mixed | 0.92x | 1.19x | 0.97x | 1.07x | 1.39x |
| `boost::shared_node_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `boost::shared_node_ptr` types | 1.02x | 1.12x | 1.08x | 0.95x | 1.21x |
| `std::unique_ptr` churn | 1.69x | 6.49x | 1.03x | 2.85x | 3.19x |
| `std::unique_ptr` bulk | 0.97x | 1.49x | 1.02x | 1.15x | 1.16x |
| `std::unique_ptr` mixed | 1.01x | 1.29x | 1.00x | 1.01x | 1.09x |
| `std::unique_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `std::unique_ptr` types | 1.05x | 1.35x | 1.03x | 0.95x | 1.14x |
| `std::shared_ptr` churn | 2.11x | 7.50x | 0.92x | 1.92x | 2.43x |
| `std::shared_ptr` bulk | 0.98x | 1.70x | 0.97x | 1.15x | 1.15x |
| `std::shared_ptr` mixed | 1.00x | 1.44x | 1.06x | 1.08x | 1.10x |
| `std::shared_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `std::shared_ptr` types | 1.18x | 1.63x | 0.99x | 0.90x | 1.15x |

with zero: threads + zero -> no threads + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `allocator alone` raw_churn | *1.57x* | *5.28x* | *1.09x* | *4.18x* | *3.59x* |
| `allocator alone` raw_batch | *0.99x* | *1.16x* | *1.00x* | *1.16x* | *1.15x* |
| `boost::root_ptr` churn | 1.90x | 4.32x | 2.47x | 5.49x | 5.58x |
| `boost::root_ptr` bulk | 0.97x | 1.63x | 1.61x | 1.71x | 1.69x |
| `boost::root_ptr` mixed | 0.98x | 1.41x | 1.11x | 1.40x | 1.38x |
| `boost::root_ptr` cycles | 0.99x | 2.12x | 2.11x | 2.74x | 2.83x |
| `boost::root_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `boost::root_ptr` types | 1.19x | 1.63x | 1.73x | 1.10x | 1.68x |
| `boost::shared_node_ptr` churn | 1.35x | 1.37x | 1.06x | 1.74x | 1.59x |
| `boost::shared_node_ptr` bulk | 1.00x | 1.26x | 1.13x | 1.19x | 1.24x |
| `boost::shared_node_ptr` mixed | 1.00x | 1.16x | 1.00x | 1.17x | 1.13x |
| `boost::shared_node_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `boost::shared_node_ptr` types | 1.05x | 1.13x | 1.25x | 0.96x | 1.19x |
| `std::unique_ptr` churn | *1.69x* | *6.49x* | *1.03x* | *2.85x* | *3.19x* |
| `std::unique_ptr` bulk | *0.97x* | *1.49x* | *1.02x* | *1.15x* | *1.16x* |
| `std::unique_ptr` mixed | *1.01x* | *1.29x* | *1.00x* | *1.01x* | *1.09x* |
| `std::unique_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `std::unique_ptr` types | *1.05x* | *1.35x* | *1.03x* | *0.95x* | *1.14x* |
| `std::shared_ptr` churn | *2.11x* | *7.50x* | *0.92x* | *1.92x* | *2.43x* |
| `std::shared_ptr` bulk | *0.98x* | *1.70x* | *0.97x* | *1.15x* | *1.15x* |
| `std::shared_ptr` mixed | *1.00x* | *1.44x* | *1.06x* | *1.08x* | *1.10x* |
| `std::shared_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `std::shared_ptr` types | *1.18x* | *1.63x* | *0.99x* | *0.90x* | *1.15x* |

### Striped locks (BOOST_ROOT_PTR_STRIPED_LOCKS)

default: threads (default) -> threads + striped

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 0.56x | 0.45x | 0.49x | 0.50x | 0.48x |
| `boost::root_ptr` bulk | 1.00x | 0.58x | 0.57x | 0.56x | 0.55x |
| `boost::root_ptr` mixed | 1.02x | 0.64x | 0.64x | 0.56x | 0.60x |
| `boost::root_ptr` cycles | 0.99x | 0.32x | 0.33x | 0.31x | 0.31x |
| `boost::root_ptr` threads | 1.43x | 1.38x | 10.55x | 1.41x | 1.36x |
| `boost::root_ptr` types | 0.71x | 0.50x | 0.58x | 0.84x | 0.54x |
| `boost::shared_node_ptr` churn | 1.16x | 0.97x | 0.85x | 1.02x | 1.22x |
| `boost::shared_node_ptr` bulk | 1.01x | 1.01x | 0.99x | 1.04x | 0.99x |
| `boost::shared_node_ptr` mixed | 0.99x | 1.01x | 0.84x | 1.01x | 1.12x |
| `boost::shared_node_ptr` threads | 1.06x | 1.04x | 0.82x | 1.07x | 1.04x |
| `boost::shared_node_ptr` types | 0.94x | 0.99x | 0.98x | 1.01x | 1.00x |

with caches: threads + caches -> threads + striped + caches

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 0.64x | 0.50x | 0.50x | 0.40x | 0.42x |
| `boost::root_ptr` bulk | 0.98x | 0.55x | 0.55x | 0.55x | 0.56x |
| `boost::root_ptr` mixed | 0.99x | 0.64x | 0.65x | 0.57x | 0.59x |
| `boost::root_ptr` cycles | 1.00x | 0.32x | 0.33x | 0.30x | 0.29x |
| `boost::root_ptr` threads | 1.51x | 1.33x | 8.40x | 10.02x | 10.24x |
| `boost::root_ptr` types | 0.73x | 0.56x | 0.57x | 0.65x | 0.54x |
| `boost::shared_node_ptr` churn | 1.04x | 0.99x | 1.13x | 1.01x | 1.01x |
| `boost::shared_node_ptr` bulk | 1.01x | 1.01x | 1.05x | 1.02x | 0.98x |
| `boost::shared_node_ptr` mixed | 1.00x | 1.14x | 0.98x | 0.99x | 0.97x |
| `boost::shared_node_ptr` threads | 1.06x | 1.06x | 0.97x | 1.17x | 1.00x |
| `boost::shared_node_ptr` types | 1.02x | 0.93x | 1.00x | 0.91x | 1.09x |

with zero: threads + zero -> threads + striped + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 0.55x | 0.56x | 0.48x | 0.56x | 0.51x |
| `boost::root_ptr` bulk | 0.99x | 0.57x | 0.57x | 0.56x | 0.56x |
| `boost::root_ptr` mixed | 0.95x | 0.65x | 0.57x | 0.60x | 0.59x |
| `boost::root_ptr` cycles | 0.99x | 0.29x | 0.34x | 0.32x | 0.32x |
| `boost::root_ptr` threads | 1.46x | 1.33x | 8.29x | 1.42x | 1.35x |
| `boost::root_ptr` types | 0.73x | 0.58x | 0.59x | 0.81x | 0.57x |
| `boost::shared_node_ptr` churn | 1.06x | 1.03x | 1.07x | 0.98x | 1.00x |
| `boost::shared_node_ptr` bulk | 1.00x | 1.02x | 1.00x | 1.00x | 0.99x |
| `boost::shared_node_ptr` mixed | 1.02x | 1.13x | 0.99x | 1.16x | 1.02x |
| `boost::shared_node_ptr` threads | 1.03x | 1.11x | 1.03x | 0.97x | 1.01x |
| `boost::shared_node_ptr` types | 1.02x | 1.03x | 1.01x | 0.91x | 1.01x |

with caches + zero: threads + caches + zero -> threads + striped + caches + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 0.61x | 0.51x | 0.46x | 0.42x | 0.45x |
| `boost::root_ptr` bulk | 1.00x | 0.54x | 0.58x | 0.57x | 0.57x |
| `boost::root_ptr` mixed | 1.03x | 0.62x | 0.65x | 0.63x | 0.55x |
| `boost::root_ptr` cycles | 0.99x | 0.33x | 0.34x | 0.29x | 0.29x |
| `boost::root_ptr` threads | 1.48x | 1.37x | 9.46x | 9.24x | 8.68x |
| `boost::root_ptr` types | 0.72x | 0.56x | 0.57x | 0.75x | 0.53x |
| `boost::shared_node_ptr` churn | 0.95x | 0.99x | 0.92x | 0.96x | 1.00x |
| `boost::shared_node_ptr` bulk | 1.01x | 0.99x | 1.02x | 0.99x | 1.03x |
| `boost::shared_node_ptr` mixed | 1.02x | 1.12x | 1.15x | 1.06x | 1.09x |
| `boost::shared_node_ptr` threads | 1.00x | 1.02x | 0.83x | 1.02x | 1.29x |
| `boost::shared_node_ptr` types | 0.95x | 1.02x | 0.97x | 0.97x | 1.05x |

### Per-thread caches (BOOST_PAGE_ALLOCATOR_THREAD_CACHE)

default: threads (default) -> threads + caches

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `allocator alone` raw_churn | 1.03x | 1.05x | 1.08x | 3.15x | 3.10x |
| `allocator alone` raw_batch | 0.99x | 1.01x | 1.01x | 1.02x | 1.05x |
| `boost::root_ptr` churn | 0.97x | 1.00x | 0.98x | 1.31x | 1.29x |
| `boost::root_ptr` bulk | 1.01x | 1.01x | 1.05x | 1.00x | 1.02x |
| `boost::root_ptr` mixed | 1.04x | 1.05x | 0.97x | 0.95x | 1.09x |
| `boost::root_ptr` cycles | 1.01x | 1.01x | 1.00x | 1.05x | 1.03x |
| `boost::root_ptr` threads | 0.96x | 1.02x | 1.09x | 1.43x | 1.26x |
| `boost::root_ptr` types | 0.96x | 1.00x | 1.00x | 1.13x | 1.02x |
| `boost::shared_node_ptr` churn | 1.21x | 0.98x | 0.90x | 1.32x | 1.60x |
| `boost::shared_node_ptr` bulk | 1.01x | 1.00x | 0.96x | 0.98x | 0.98x |
| `boost::shared_node_ptr` mixed | 0.99x | 1.03x | 0.92x | 0.95x | 1.14x |
| `boost::shared_node_ptr` threads | 1.05x | 1.00x | 0.84x | 18.02x | 19.28x |
| `boost::shared_node_ptr` types | 0.97x | 1.14x | 0.96x | 0.97x | 1.00x |
| `std::unique_ptr` churn | 0.96x | 0.95x | 0.88x | 2.21x | 2.90x |
| `std::unique_ptr` bulk | 1.01x | 1.01x | 1.00x | 1.07x | 1.11x |
| `std::unique_ptr` mixed | 1.03x | 1.09x | 1.03x | 0.89x | 1.04x |
| `std::unique_ptr` threads | 1.04x | 1.01x | 0.99x | 86.68x | 53.18x |
| `std::unique_ptr` types | 0.96x | 1.01x | 0.98x | 0.99x | 1.04x |
| `std::shared_ptr` churn | 0.89x | 0.90x | 0.81x | 1.99x | 2.67x |
| `std::shared_ptr` bulk | 0.99x | 1.00x | 0.97x | 1.05x | 1.04x |
| `std::shared_ptr` mixed | 1.01x | 1.05x | 1.17x | 0.99x | 1.01x |
| `std::shared_ptr` threads | 0.99x | 1.04x | 1.04x | 56.16x | 38.28x |
| `std::shared_ptr` types | 0.98x | 1.15x | 0.98x | 0.89x | 1.13x |

with striped: threads + striped -> threads + striped + caches

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `allocator alone` raw_churn | *1.03x* | *1.05x* | *1.08x* | *3.15x* | *3.10x* |
| `allocator alone` raw_batch | *0.99x* | *1.01x* | *1.01x* | *1.02x* | *1.05x* |
| `boost::root_ptr` churn | 1.11x | 1.12x | 1.01x | 1.05x | 1.13x |
| `boost::root_ptr` bulk | 0.99x | 0.97x | 1.01x | 0.98x | 1.03x |
| `boost::root_ptr` mixed | 1.01x | 1.06x | 0.99x | 0.95x | 1.07x |
| `boost::root_ptr` cycles | 1.01x | 1.02x | 0.98x | 1.02x | 0.97x |
| `boost::root_ptr` threads | 1.01x | 0.98x | 0.86x | 10.19x | 9.48x |
| `boost::root_ptr` types | 0.99x | 1.12x | 0.98x | 0.88x | 1.03x |
| `boost::shared_node_ptr` churn | 1.08x | 1.00x | 1.19x | 1.31x | 1.33x |
| `boost::shared_node_ptr` bulk | 1.01x | 1.00x | 1.02x | 0.96x | 0.97x |
| `boost::shared_node_ptr` mixed | 1.00x | 1.16x | 1.07x | 0.94x | 0.98x |
| `boost::shared_node_ptr` threads | 1.05x | 1.01x | 1.00x | 19.57x | 18.59x |
| `boost::shared_node_ptr` types | 1.05x | 1.07x | 0.99x | 0.89x | 1.10x |
| `std::unique_ptr` churn | *0.96x* | *0.95x* | *0.88x* | *2.21x* | *2.90x* |
| `std::unique_ptr` bulk | *1.01x* | *1.01x* | *1.00x* | *1.07x* | *1.11x* |
| `std::unique_ptr` mixed | *1.03x* | *1.09x* | *1.03x* | *0.89x* | *1.04x* |
| `std::unique_ptr` threads | *1.04x* | *1.01x* | *0.99x* | *86.68x* | *53.18x* |
| `std::unique_ptr` types | *0.96x* | *1.01x* | *0.98x* | *0.99x* | *1.04x* |
| `std::shared_ptr` churn | *0.89x* | *0.90x* | *0.81x* | *1.99x* | *2.67x* |
| `std::shared_ptr` bulk | *0.99x* | *1.00x* | *0.97x* | *1.05x* | *1.04x* |
| `std::shared_ptr` mixed | *1.01x* | *1.05x* | *1.17x* | *0.99x* | *1.01x* |
| `std::shared_ptr` threads | *0.99x* | *1.04x* | *1.04x* | *56.16x* | *38.28x* |
| `std::shared_ptr` types | *0.98x* | *1.15x* | *0.98x* | *0.89x* | *1.13x* |

with zero: threads + zero -> threads + caches + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `allocator alone` raw_churn | *1.03x* | *1.05x* | *1.08x* | *3.15x* | *3.10x* |
| `allocator alone` raw_batch | *0.99x* | *1.01x* | *1.01x* | *1.02x* | *1.05x* |
| `boost::root_ptr` churn | 0.98x | 1.07x | 1.02x | 1.46x | 1.25x |
| `boost::root_ptr` bulk | 0.98x | 1.03x | 1.00x | 0.96x | 0.95x |
| `boost::root_ptr` mixed | 1.00x | 1.04x | 0.86x | 0.92x | 0.98x |
| `boost::root_ptr` cycles | 0.99x | 1.01x | 1.02x | 1.05x | 1.09x |
| `boost::root_ptr` threads | 1.01x | 0.96x | 0.87x | 1.37x | 1.47x |
| `boost::root_ptr` types | 1.05x | 1.03x | 1.00x | 0.94x | 1.05x |
| `boost::shared_node_ptr` churn | 1.03x | 1.03x | 1.07x | 1.36x | 1.29x |
| `boost::shared_node_ptr` bulk | 0.99x | 1.04x | 1.01x | 0.95x | 0.95x |
| `boost::shared_node_ptr` mixed | 1.00x | 1.02x | 0.95x | 1.04x | 0.97x |
| `boost::shared_node_ptr` threads | 1.07x | 1.10x | 1.28x | 17.46x | 16.42x |
| `boost::shared_node_ptr` types | 1.00x | 0.99x | 1.04x | 0.91x | 1.00x |
| `std::unique_ptr` churn | *0.96x* | *0.95x* | *0.88x* | *2.21x* | *2.90x* |
| `std::unique_ptr` bulk | *1.01x* | *1.01x* | *1.00x* | *1.07x* | *1.11x* |
| `std::unique_ptr` mixed | *1.03x* | *1.09x* | *1.03x* | *0.89x* | *1.04x* |
| `std::unique_ptr` threads | *1.04x* | *1.01x* | *0.99x* | *86.68x* | *53.18x* |
| `std::unique_ptr` types | *0.96x* | *1.01x* | *0.98x* | *0.99x* | *1.04x* |
| `std::shared_ptr` churn | *0.89x* | *0.90x* | *0.81x* | *1.99x* | *2.67x* |
| `std::shared_ptr` bulk | *0.99x* | *1.00x* | *0.97x* | *1.05x* | *1.04x* |
| `std::shared_ptr` mixed | *1.01x* | *1.05x* | *1.17x* | *0.99x* | *1.01x* |
| `std::shared_ptr` threads | *0.99x* | *1.04x* | *1.04x* | *56.16x* | *38.28x* |
| `std::shared_ptr` types | *0.98x* | *1.15x* | *0.98x* | *0.89x* | *1.13x* |

with striped + zero: threads + striped + zero -> threads + striped + caches + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `allocator alone` raw_churn | *1.03x* | *1.05x* | *1.08x* | *3.15x* | *3.10x* |
| `allocator alone` raw_batch | *0.99x* | *1.01x* | *1.01x* | *1.02x* | *1.05x* |
| `boost::root_ptr` churn | 1.07x | 0.97x | 0.98x | 1.10x | 1.11x |
| `boost::root_ptr` bulk | 1.00x | 0.99x | 1.02x | 0.97x | 0.98x |
| `boost::root_ptr` mixed | 1.08x | 1.00x | 0.97x | 0.96x | 0.92x |
| `boost::root_ptr` cycles | 1.00x | 1.15x | 1.03x | 0.95x | 1.00x |
| `boost::root_ptr` threads | 1.03x | 0.99x | 0.99x | 8.91x | 9.39x |
| `boost::root_ptr` types | 1.03x | 0.99x | 0.97x | 0.87x | 0.97x |
| `boost::shared_node_ptr` churn | 0.92x | 0.99x | 0.93x | 1.34x | 1.29x |
| `boost::shared_node_ptr` bulk | 1.00x | 1.00x | 1.03x | 0.95x | 0.98x |
| `boost::shared_node_ptr` mixed | 1.00x | 1.02x | 1.11x | 0.95x | 1.04x |
| `boost::shared_node_ptr` threads | 1.04x | 1.01x | 1.03x | 18.40x | 20.85x |
| `boost::shared_node_ptr` types | 0.94x | 0.98x | 1.00x | 0.98x | 1.03x |
| `std::unique_ptr` churn | *0.96x* | *0.95x* | *0.88x* | *2.21x* | *2.90x* |
| `std::unique_ptr` bulk | *1.01x* | *1.01x* | *1.00x* | *1.07x* | *1.11x* |
| `std::unique_ptr` mixed | *1.03x* | *1.09x* | *1.03x* | *0.89x* | *1.04x* |
| `std::unique_ptr` threads | *1.04x* | *1.01x* | *0.99x* | *86.68x* | *53.18x* |
| `std::unique_ptr` types | *0.96x* | *1.01x* | *0.98x* | *0.99x* | *1.04x* |
| `std::shared_ptr` churn | *0.89x* | *0.90x* | *0.81x* | *1.99x* | *2.67x* |
| `std::shared_ptr` bulk | *0.99x* | *1.00x* | *0.97x* | *1.05x* | *1.04x* |
| `std::shared_ptr` mixed | *1.01x* | *1.05x* | *1.17x* | *0.99x* | *1.01x* |
| `std::shared_ptr` threads | *0.99x* | *1.04x* | *1.04x* | *56.16x* | *38.28x* |
| `std::shared_ptr` types | *0.98x* | *1.15x* | *0.98x* | *0.89x* | *1.13x* |

### Zeroization (BOOST_ZEROIZATION)

default: threads (default) -> threads + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 1.03x | 0.90x | 1.09x | 0.90x | 0.98x |
| `boost::root_ptr` bulk | 1.01x | 1.00x | 1.00x | 1.00x | 1.03x |
| `boost::root_ptr` mixed | 1.01x | 1.03x | 1.15x | 0.95x | 1.07x |
| `boost::root_ptr` cycles | 1.00x | 1.11x | 1.00x | 1.01x | 0.94x |
| `boost::root_ptr` threads | 0.97x | 1.03x | 1.12x | 0.96x | 0.96x |
| `boost::root_ptr` types | 0.93x | 0.85x | 1.00x | 1.05x | 0.96x |
| `boost::shared_node_ptr` churn | 1.16x | 0.97x | 0.94x | 1.01x | 1.22x |
| `boost::shared_node_ptr` bulk | 1.00x | 0.98x | 0.97x | 1.04x | 1.00x |
| `boost::shared_node_ptr` mixed | 0.98x | 1.03x | 0.97x | 0.90x | 1.07x |
| `boost::shared_node_ptr` threads | 1.00x | 0.90x | 0.79x | 1.05x | 1.10x |
| `boost::shared_node_ptr` types | 0.97x | 0.98x | 0.96x | 0.98x | 1.02x |

with striped: threads + striped -> threads + striped + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 1.02x | 1.12x | 1.07x | 1.01x | 1.03x |
| `boost::root_ptr` bulk | 0.99x | 0.99x | 0.99x | 1.00x | 1.04x |
| `boost::root_ptr` mixed | 0.94x | 1.05x | 1.02x | 1.01x | 1.06x |
| `boost::root_ptr` cycles | 0.99x | 1.00x | 1.00x | 1.02x | 0.96x |
| `boost::root_ptr` threads | 1.00x | 0.99x | 0.88x | 0.97x | 0.96x |
| `boost::root_ptr` types | 0.97x | 0.99x | 1.00x | 1.02x | 1.02x |
| `boost::shared_node_ptr` churn | 1.06x | 1.03x | 1.18x | 0.97x | 1.00x |
| `boost::shared_node_ptr` bulk | 0.99x | 0.99x | 0.97x | 1.00x | 1.00x |
| `boost::shared_node_ptr` mixed | 1.01x | 1.15x | 1.14x | 1.03x | 0.97x |
| `boost::shared_node_ptr` threads | 0.98x | 0.96x | 0.99x | 0.94x | 1.07x |
| `boost::shared_node_ptr` types | 1.05x | 1.02x | 0.99x | 0.89x | 1.04x |

with caches: threads + caches -> threads + caches + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 1.03x | 0.97x | 1.13x | 1.00x | 0.95x |
| `boost::root_ptr` bulk | 0.98x | 1.02x | 0.96x | 0.96x | 0.97x |
| `boost::root_ptr` mixed | 0.97x | 1.01x | 1.01x | 0.92x | 0.96x |
| `boost::root_ptr` cycles | 0.99x | 1.11x | 1.01x | 1.01x | 0.99x |
| `boost::root_ptr` threads | 1.03x | 0.96x | 0.90x | 0.92x | 1.12x |
| `boost::root_ptr` types | 1.02x | 0.88x | 1.00x | 0.87x | 0.98x |
| `boost::shared_node_ptr` churn | 0.98x | 1.02x | 1.12x | 1.04x | 0.98x |
| `boost::shared_node_ptr` bulk | 0.98x | 1.01x | 1.01x | 1.01x | 0.97x |
| `boost::shared_node_ptr` mixed | 0.99x | 1.03x | 1.00x | 0.98x | 0.91x |
| `boost::shared_node_ptr` threads | 1.02x | 0.99x | 1.20x | 1.01x | 0.93x |
| `boost::shared_node_ptr` types | 1.01x | 0.85x | 1.04x | 0.92x | 1.02x |

with striped + caches: threads + striped + caches -> threads + striped + caches + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 0.99x | 0.98x | 1.03x | 1.06x | 1.00x |
| `boost::root_ptr` bulk | 1.00x | 1.01x | 1.00x | 1.00x | 0.99x |
| `boost::root_ptr` mixed | 1.01x | 0.99x | 1.01x | 1.02x | 0.91x |
| `boost::root_ptr` cycles | 0.98x | 1.14x | 1.05x | 0.96x | 0.99x |
| `boost::root_ptr` threads | 1.01x | 0.99x | 1.01x | 0.85x | 0.95x |
| `boost::root_ptr` types | 1.01x | 0.87x | 0.99x | 1.00x | 0.96x |
| `boost::shared_node_ptr` churn | 0.90x | 1.01x | 0.92x | 0.99x | 0.97x |
| `boost::shared_node_ptr` bulk | 0.98x | 0.99x | 0.99x | 0.98x | 1.02x |
| `boost::shared_node_ptr` mixed | 1.00x | 1.02x | 1.18x | 1.05x | 1.02x |
| `boost::shared_node_ptr` threads | 0.97x | 0.96x | 1.03x | 0.89x | 1.20x |
| `boost::shared_node_ptr` types | 0.94x | 0.93x | 1.00x | 0.98x | 0.98x |

no threads: no threads -> no threads + zero

| pointer, scenario | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| `boost::root_ptr` churn | 0.89x | 0.72x | 1.17x | 0.79x | 0.88x |
| `boost::root_ptr` bulk | 1.00x | 0.96x | 1.03x | 0.97x | 0.98x |
| `boost::root_ptr` mixed | 1.01x | 0.95x | 0.93x | 0.96x | 0.84x |
| `boost::root_ptr` cycles | 0.99x | 1.01x | 0.81x | 0.97x | 0.97x |
| `boost::root_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `boost::root_ptr` types | 0.89x | 1.02x | 1.13x | 0.98x | 0.92x |
| `boost::shared_node_ptr` churn | 1.00x | 0.79x | 0.99x | 0.98x | 0.90x |
| `boost::shared_node_ptr` bulk | 0.98x | 1.02x | 1.03x | 0.98x | 1.02x |
| `boost::shared_node_ptr` mixed | 1.06x | 1.00x | 1.00x | 0.98x | 0.86x |
| `boost::shared_node_ptr` threads | n/a | n/a | n/a | n/a | n/a |
| `boost::shared_node_ptr` types | 1.01x | 0.99x | 1.10x | 0.99x | 1.00x |

## Fastest allocator

For every build, pointer and scenario: the fastest allocator and its time / the time of `page_allocator_by_size` (the default) in the same cell (lower is better; 1.00 = the default is the fastest).

| pointer, scenario | threads (default) | threads + striped | threads + caches | threads + striped + caches | threads + zero | threads + striped + zero | threads + caches + zero | threads + striped + caches + zero | no threads | no threads + zero |
|---|---|---|---|---|---|---|---|---|---|---|
| `allocator alone` raw_churn | page_type 0.98 | *page_type 0.98* | page_type 0.97 | *page_type 0.97* | *page_type 0.98* | *page_type 0.98* | *page_type 0.97* | *page_type 0.97* | fast 0.70 | *fast 0.70* |
| `allocator alone` raw_batch | page_type 1.00 | *page_type 1.00* | page_size | *page_size* | *page_type 1.00* | *page_type 1.00* | *page_size* | *page_size* | page_type 0.99 | *page_type 0.99* |
| `boost::root_ptr` churn | fast 0.99 | page_type 0.98 | page_type 0.99 | page_size | std 0.91 | std 0.97 | page_type 0.94 | page_type 1.00 | page_type 1.00 | page_size |
| `boost::root_ptr` bulk | page_type 0.99 | page_type 0.97 | page_size | page_size | page_size | page_size | fast 0.99 | page_size | page_size | page_size |
| `boost::root_ptr` mixed | page_type 0.88 | page_type 0.94 | page_size | page_size | page_type 1.00 | page_type 0.98 | page_size | page_type 0.94 | page_size | page_type 0.99 |
| `boost::root_ptr` cycles | page_size | page_size | page_size | page_type 0.98 | fast 0.94 | page_type 0.96 | page_type 1.00 | fast 0.90 | page_type 0.99 | page_type 1.00 |
| `boost::root_ptr` threads | std 0.81 | std 0.10 | page_type 0.91 | page_type 0.93 | std 0.69 | std 0.11 | page_size | page_size | n/a | n/a |
| `boost::root_ptr` types | fast 0.97 | page_size | fast 0.99 | fast 0.96 | page_size | page_size | page_size | page_size | page_size | page_size |
| `boost::shared_node_ptr` churn | std 0.75 | fast 0.95 | page_size | page_size | fast 0.95 | std 0.91 | page_type 0.95 | page_type 0.98 | fast 0.97 | page_type 0.91 |
| `boost::shared_node_ptr` bulk | page_size | page_type 0.99 | page_size | page_type 0.99 | page_type 1.00 | page_type 1.00 | page_type 0.99 | page_size | page_type 1.00 | page_size |
| `boost::shared_node_ptr` mixed | page_type 0.88 | page_type 0.98 | page_size | fast 0.99 | page_size | page_type 0.92 | page_type 0.98 | fast 1.00 | page_size | page_size |
| `boost::shared_node_ptr` threads | std 0.05 | std 0.07 | page_size | page_type 0.89 | std 0.07 | std 0.07 | std 0.95 | page_size | n/a | n/a |
| `boost::shared_node_ptr` types | page_size | page_size | fast 0.88 | page_size | page_size | page_size | page_size | page_size | page_size | page_size |
| `std::unique_ptr` churn | std 0.88 | *std 0.88* | page_size | *page_size* | *std 0.88* | *std 0.88* | *page_size* | *page_size* | fast 0.69 | *fast 0.69* |
| `std::unique_ptr` bulk | page_type 1.00 | *page_type 1.00* | page_size | *page_size* | *page_type 1.00* | *page_type 1.00* | *page_size* | *page_size* | page_size | *page_size* |
| `std::unique_ptr` mixed | page_type 0.87 | *page_type 0.87* | page_size | *page_size* | *page_type 0.87* | *page_type 0.87* | *page_size* | *page_size* | page_type 0.93 | *page_type 0.93* |
| `std::unique_ptr` threads | std 0.03 | *std 0.03* | page_type 0.57 | *page_type 0.57* | *std 0.03* | *std 0.03* | *page_type 0.57* | *page_type 0.57* | n/a | n/a |
| `std::unique_ptr` types | page_size | *page_size* | page_size | *page_size* | *page_size* | *page_size* | *page_size* | *page_size* | fast 0.96 | *fast 0.96* |
| `std::shared_ptr` churn | std 0.78 | *std 0.78* | page_size | *page_size* | *std 0.78* | *std 0.78* | *page_size* | *page_size* | fast 0.62 | *fast 0.62* |
| `std::shared_ptr` bulk | page_size | *page_size* | page_type 1.00 | *page_type 1.00* | *page_size* | *page_size* | *page_type 1.00* | *page_type 1.00* | page_size | *page_size* |
| `std::shared_ptr` mixed | page_type 0.98 | *page_type 0.98* | page_type 1.00 | *page_type 1.00* | *page_type 0.98* | *page_type 0.98* | *page_type 1.00* | *page_type 1.00* | page_size | *page_size* |
| `std::shared_ptr` threads | std 0.04 | *std 0.04* | page_type 0.62 | *page_type 0.62* | *std 0.04* | *std 0.04* | *page_type 0.62* | *page_type 0.62* | n/a | n/a |
| `std::shared_ptr` types | page_size | *page_size* | page_size | *page_size* | *page_size* | *page_size* | *page_size* | *page_size* | fast 0.92 | *fast 0.92* |

## The other pointers against `root_ptr`

Time of the pointer / time of `boost::root_ptr` in the same build, allocator and scenario (lower is better: below 1.00x the pointer is faster than `root_ptr`).

With `boost::page_allocator_by_size`:

| pointer, scenario | threads (default) | threads + striped | threads + caches | threads + striped + caches | threads + zero | threads + striped + zero | threads + caches + zero | threads + striped + caches + zero | no threads | no threads + zero |
|---|---|---|---|---|---|---|---|---|---|---|
| `boost::shared_node_ptr` churn | 1.07x | 0.42x | 0.86x | 0.36x | 0.85x | 0.44x | 0.83x | 0.37x | 3.05x | 3.00x |
| `std::unique_ptr` churn | 0.32x | *0.15x* | 0.14x | *0.06x* | *0.31x* | *0.16x* | *0.14x* | *0.06x* | 0.62x | *0.55x* |
| `std::shared_ptr` churn | 0.39x | *0.19x* | 0.19x | *0.08x* | *0.38x* | *0.19x* | *0.18x* | *0.08x* | 0.99x | *0.88x* |
| `boost::shared_node_ptr` bulk | 1.06x | 0.60x | 1.11x | 0.64x | 1.10x | 0.61x | 1.11x | 0.61x | 1.56x | 1.50x |
| `std::unique_ptr` bulk | 0.36x | *0.20x* | 0.33x | *0.18x* | *0.37x* | *0.21x* | *0.32x* | *0.18x* | 0.55x | *0.54x* |
| `std::shared_ptr` bulk | 0.48x | *0.26x* | 0.47x | *0.26x* | *0.49x* | *0.27x* | *0.45x* | *0.26x* | 0.74x | *0.72x* |
| `boost::shared_node_ptr` mixed | 1.14x | 0.61x | 1.09x | 0.66x | 1.14x | 0.66x | 1.15x | 0.59x | 1.45x | 1.40x |
| `std::unique_ptr` mixed | 0.47x | *0.28x* | 0.49x | *0.29x* | *0.50x* | *0.30x* | *0.48x* | *0.26x* | 0.77x | *0.64x* |
| `std::shared_ptr` mixed | 0.59x | *0.35x* | 0.64x | *0.37x* | *0.63x* | *0.37x* | *0.62x* | *0.34x* | 0.95x | *0.79x* |
| `boost::shared_node_ptr` threads | 0.61x | 0.80x | 0.04x | 0.41x | 0.54x | 0.72x | 0.05x | 0.32x | n/a | n/a |
| `std::unique_ptr` threads | 0.47x | *0.64x* | 0.01x | *0.11x* | *0.45x* | *0.61x* | *0.01x* | *0.11x* | n/a | n/a |
| `std::shared_ptr` threads | 0.43x | *0.58x* | 0.01x | *0.14x* | *0.41x* | *0.56x* | *0.02x* | *0.14x* | n/a | n/a |
| `boost::shared_node_ptr` types | 1.15x | 0.63x | 1.17x | 0.59x | 1.08x | 0.61x | 1.14x | 0.58x | 1.66x | 1.53x |
| `std::unique_ptr` types | 0.45x | *0.24x* | 0.44x | *0.24x* | *0.43x* | *0.25x* | *0.44x* | *0.23x* | 0.69x | *0.64x* |
| `std::shared_ptr` types | 0.59x | *0.32x* | 0.53x | *0.29x* | *0.57x* | *0.32x* | *0.53x* | *0.28x* | 0.90x | *0.83x* |

With `std::allocator`:

| pointer, scenario | threads (default) | threads + striped | threads + caches | threads + striped + caches | threads + zero | threads + striped + zero | threads + caches + zero | threads + striped + caches + zero | no threads | no threads + zero |
|---|---|---|---|---|---|---|---|---|---|---|
| `boost::shared_node_ptr` churn | 0.79x | 0.45x | 0.86x | 0.39x | 0.91x | 0.41x | 0.87x | 0.43x | 1.80x | 2.13x |
| `std::unique_ptr` churn | 0.28x | *0.14x* | 0.31x | *0.16x* | *0.30x* | *0.14x* | *0.35x* | *0.16x* | 0.62x | *0.72x* |
| `std::shared_ptr` churn | 0.30x | *0.15x* | 0.36x | *0.18x* | *0.33x* | *0.16x* | *0.41x* | *0.19x* | 0.74x | *0.87x* |
| `boost::shared_node_ptr` bulk | 1.04x | 0.60x | 1.13x | 0.60x | 1.07x | 0.61x | 1.07x | 0.60x | 1.53x | 1.53x |
| `std::unique_ptr` bulk | 0.46x | *0.26x* | 0.48x | *0.26x* | *0.46x* | *0.26x* | *0.46x* | *0.26x* | 0.71x | *0.72x* |
| `std::shared_ptr` bulk | 0.53x | *0.31x* | 0.57x | *0.32x* | *0.54x* | *0.30x* | *0.55x* | *0.32x* | 0.87x | *0.89x* |
| `boost::shared_node_ptr` mixed | 1.08x | 0.83x | 1.15x | 0.76x | 1.29x | 0.74x | 1.16x | 0.65x | 1.54x | 1.43x |
| `std::unique_ptr` mixed | 0.53x | *0.34x* | 0.50x | *0.32x* | *0.61x* | *0.35x* | *0.51x* | *0.33x* | 0.72x | *0.67x* |
| `std::shared_ptr` mixed | 0.69x | *0.44x* | 0.57x | *0.37x* | *0.79x* | *0.45x* | *0.58x* | *0.37x* | 0.89x | *0.83x* |
| `boost::shared_node_ptr` threads | 0.04x | 0.52x | 0.05x | 0.45x | 0.06x | 0.46x | 0.04x | 0.44x | n/a | n/a |
| `std::unique_ptr` threads | 0.02x | *0.17x* | 0.02x | *0.15x* | *0.02x* | *0.15x* | *0.02x* | *0.15x* | n/a | n/a |
| `std::shared_ptr` threads | 0.02x | *0.20x* | 0.02x | *0.17x* | *0.02x* | *0.18x* | *0.02x* | *0.17x* | n/a | n/a |
| `boost::shared_node_ptr` types | 1.06x | 0.63x | 1.11x | 0.63x | 1.10x | 0.64x | 1.06x | 0.62x | 1.49x | 1.53x |
| `std::unique_ptr` types | 0.47x | *0.27x* | 0.48x | *0.28x* | *0.47x* | *0.28x* | *0.48x* | *0.27x* | 0.70x | *0.79x* |
| `std::shared_ptr` types | 0.55x | *0.32x* | 0.56x | *0.32x* | *0.55x* | *0.32x* | *0.56x* | *0.32x* | 0.85x | *0.96x* |

## Bulk release scaling

ns per object, one run per size, threads (default) build (lower is better).

`boost::root_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12,500 | 20,982 | 119 | 120 | 112 | 111 |
| 25,000 | 37,743 | 115 | 121 | 110 | 107 |
| 50,000 | 70,148 | 206 | 145 | 124 | 118 |
| 100,000 | 143,228 | 136 | 145 | 134 | 135 |
| 200,000 | 359,653 | 138 | 142 | 132 | 133 |

`boost::shared_node_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12,500 | 20,896 | 188 | 133 | 126 | 123 |
| 25,000 | 37,860 | 124 | 123 | 122 | 140 |
| 50,000 | 70,443 | 157 | 147 | 120 | 123 |
| 100,000 | 145,512 | 151 | 156 | 160 | 148 |
| 200,000 | 401,845 | 157 | 166 | 126 | 147 |

`std::unique_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12,500 | 15,105 | 51.1 | 60.5 | 39.8 | 40.3 |
| 25,000 | 24,973 | 63.0 | 66.0 | 49.2 | 38.8 |
| 50,000 | 45,279 | 62.5 | 66.3 | 47.7 | 48.3 |
| 100,000 | 85,217 | 63.8 | 66.7 | 48.1 | 48.1 |
| 200,000 | 188,884 | 63.4 | 66.7 | 48.3 | 41.7 |

`std::shared_ptr`:

| objects | pool | fast | std | page_type | page_size |
|---|---|---|---|---|---|
| 12,500 | 20,023 | 85.6 | 66.8 | 58.1 | 55.7 |
| 25,000 | 36,720 | 86.7 | 73.0 | 55.6 | 54.9 |
| 50,000 | 69,284 | 85.5 | 79.3 | 63.3 | 63.9 |
| 100,000 | 136,703 | 98.1 | 80.6 | 64.7 | 63.8 |
| 200,000 | 315,342 | 98.3 | 82.2 | 63.5 | 66.3 |
