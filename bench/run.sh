#!/bin/bash
# Builds bench/allocbench.cpp five times - with thread support ("mt", the
# default), with BOOST_DISABLE_THREADS ("st": no root_ptr mutex, plain reference
# counts, no pool mutexes), with BOOST_ROOT_PTR_STRIPED_LOCKS ("sl": root_ptr's
# global mutex replaced by address-striped locks), with
# BOOST_PAGE_ALLOCATOR_THREAD_CACHE ("tc": per-thread caches of free blocks in
# the page allocators) and with both ("tcsl") - and prints, as Markdown:
#   1. the mt build: the allocator alone, then per pointer type (root_ptr,
#      unique_ptr, shared_ptr) the median of REPEAT runs of every scenario for
#      every allocator, time and memory; the pointer types side by side; and how
#      bulk release scales with the number of objects (one run per size);
#   2. the st build: the same time tables, single-threaded scenarios only;
#   3. what thread support costs: mt vs. st per scenario;
#   4. striped locks: mt vs. sl for root_ptr (the only pointer that uses them);
#   5. per-thread caches: mt vs. tc for every pointer, sl vs. tcsl for root_ptr.
# Every (build, pointer, allocator, scenario) runs in a fresh process. The raw
# measurements are kept in bench/raw-main.txt and bench/raw-scale.txt;
# "bench/run.sh --tables" reprints the tables from them without measuring.
# Pointers: root (boost::root_ptr), unique and shared (std::shared_ptr); the
# striped builds measure root_ptr only.
#   usage: bench/run.sh [repeat | --tables]
#          bench/run.sh --snode [repeat]
#            measures boost::shared_node_ptr (what FCXXSS_SHARED_PTR emits)
#            against boost::root_ptr, interleaved in ONE run (builds mt, st,
#            tc), into bench/raw-snode.txt: that section compares them with each
#            other only, never with the main run's rows.
set -e
SNODE=""
if [ "$1" = --snode ]; then SNODE=1; shift; fi
REPEAT="${1:-5}"
DIR="$(cd "$(dirname "$0")" && pwd)"
MAIN="$DIR/raw-main.txt"
SCALE="$DIR/raw-scale.txt"
SNODEF="$DIR/raw-snode.txt"
TMP="$(mktemp -d)"

# Every measurement waits for a cool CPU: this machine throttles (Intel
# powerclamp idle injection, 'kidle_inj' threads) under the benchmark's own
# four-thread load, which made a whole run about 3x slower in places. It waits
# while any kidle_inj thread exists or the package is at BENCH_MAX_TEMP
# (millidegrees C, default 70000) or hotter.
BENCH_MAX_TEMP="${BENCH_MAX_TEMP:-70000}"
PKG=""
for z in /sys/class/thermal/thermal_zone*; do
  [ "$(cat $z/type 2>/dev/null)" = x86_pkg_temp ] && PKG="$z/temp"
done
cool() {
  while ps -eo comm | grep -q kidle_inj ||
        { [ -n "$PKG" ] && [ "$(cat $PKG)" -ge "$BENCH_MAX_TEMP" ]; }; do
    sleep 2
  done
}
measure() { cool; "$@"; }

if [ "$REPEAT" != --tables ]; then
if [ -n "$SNODE" ]; then rm -f "$SNODEF"; else rm -f "$MAIN" "$SCALE"; fi
FLAGS="-std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include"
clang++ $FLAGS "$DIR/allocbench.cpp" -o "$TMP/mt" -lboost_thread -lpthread
clang++ $FLAGS -DBOOST_DISABLE_THREADS "$DIR/allocbench.cpp" -o "$TMP/st" -lboost_thread -lpthread
clang++ $FLAGS -DBOOST_ROOT_PTR_STRIPED_LOCKS "$DIR/allocbench.cpp" -o "$TMP/sl" -lboost_thread -lpthread
clang++ $FLAGS -DBOOST_PAGE_ALLOCATOR_THREAD_CACHE "$DIR/allocbench.cpp" -o "$TMP/tc" -lboost_thread -lpthread
clang++ $FLAGS -DBOOST_PAGE_ALLOCATOR_THREAD_CACHE -DBOOST_ROOT_PTR_STRIPED_LOCKS "$DIR/allocbench.cpp" -o "$TMP/tcsl" -lboost_thread -lpthread

POINTERS="root unique shared"
ALLOCATORS="pool fast std page_type page_size"
RAW="raw_churn raw_batch"
SCENARIOS="churn bulk mixed cycles threads types"
SIZES="12500 25000 50000 100000 200000"

if [ -n "$SNODE" ]; then
for r in $(seq 1 "$REPEAT"); do
  for b in mt st tc; do
    for s in $SCENARIOS; do
      [ "$b" = st ] && [ "$s" = threads ] && continue
      for a in $ALLOCATORS; do
        for p in root snode; do
          echo "$b $p $a $s $(measure "$TMP/$b" "$p" "$a" "$s")" >> "$SNODEF"
        done
      done
    done
  done
done
else
for r in $(seq 1 "$REPEAT"); do
  for b in mt st sl tc tcsl; do
    case $b in sl|tcsl) ;; *) for s in $RAW; do
      for a in $ALLOCATORS; do      # interleaved, so drift hits every allocator alike
        echo "$b raw $a $s $(measure "$TMP/$b" root "$a" "$s")" >> "$MAIN"
      done
    done ;; esac
    for p in $POINTERS; do
      case $b in sl|tcsl) [ "$p" != root ] && continue ;; esac
      for s in $SCENARIOS; do
        [ "$b" = st ] && [ "$s" = threads ] && continue
        for a in $ALLOCATORS; do
          echo "$b $p $a $s $(measure "$TMP/$b" "$p" "$a" "$s")" >> "$MAIN"
        done
      done
    done
  done
done
for p in $POINTERS; do
  for n in $SIZES; do
    for a in $ALLOCATORS; do
      echo "mt $p $a $n $(measure "$TMP/mt" "$p" "$a" bulk "$n")" >> "$SCALE"
    done
  done
done
fi
fi

python3 - "$MAIN" "$SCALE" "$SNODEF" <<'EOF'
import sys, statistics, collections
alloc = ['pool', 'fast', 'std', 'page_type', 'page_size']
ptrs = [('root', 'boost::root_ptr'), ('unique', 'std::unique_ptr'), ('shared', 'std::shared_ptr')]
t = collections.defaultdict(list); m = collections.defaultdict(list); na = set()
for line in open(sys.argv[1]):
    f = line.split()
    key = tuple(f[:4])                      # build, pointer, allocator, scenario
    if f[4] == 'n/a': na.add(key); continue
    t[key].append(float(f[4]))
    if len(f) > 5: m[key].append(int(f[5]))
med = lambda k: statistics.median(t[k])
head = lambda first, cols: '| ' + first + ' | ' + ' | '.join(cols) + ' |\n' + '|---' * (len(cols) + 1) + '|'

def time_table(b, p, scenarios):
    print(head('scenario', alloc))
    for s in scenarios:
        if (b, p, 'pool', s) in na:
            print('| %s | %s |' % (s, ' | '.join('n/a' for a in alloc))); continue
        base = med((b, p, 'pool', s))
        print('| %s | %s |' % (s, ' | '.join('%.1f (%.2fx)' % (med((b, p, a, s)), base / med((b, p, a, s))) for a in alloc)))

scen = ['churn', 'bulk', 'mixed', 'cycles', 'threads', 'types']
single = [s for s in scen if s != 'threads']

print('## Built with thread support (the default)\n')
print('### Allocator alone\n\nTime, ns per operation (speed-up over `pool` in parentheses):\n')
time_table('mt', 'raw', ['raw_churn', 'raw_batch'])
for p, name in ptrs:
    print('\n### `%s`\n\nTime, ns per operation (speed-up over `pool` in parentheses):\n' % name)
    time_table('mt', p, scen)
    print('\nResident memory grown while the objects are live, kB:\n')
    print(head('scenario', alloc))
    for s in ['bulk', 'mixed', 'types']:
        print('| %s | %s |' % (s, ' | '.join('%d' % statistics.median(m[('mt', p, a, s)]) for a in alloc)))

print('\n### Pointer types side by side\n\nTime, ns per operation, `root_ptr` / `unique_ptr` / `shared_ptr`:\n')
print(head('scenario', alloc))
for s in scen:
    print('| %s | %s |' % (s, ' | '.join(' / '.join('n/a' if ('mt', p, a, s) in na else '%.1f' % med(('mt', p, a, s))
                                                      for p, _ in ptrs) for a in alloc)))
print('\nResident memory grown, kB, `root_ptr` / `unique_ptr` / `shared_ptr`:\n')
print(head('scenario', alloc))
for s in ['bulk', 'mixed', 'types']:
    print('| %s | %s |' % (s, ' | '.join(' / '.join('%d' % statistics.median(m[('mt', p, a, s)]) for p, _ in ptrs) for a in alloc)))

sc = collections.defaultdict(dict)
for line in open(sys.argv[2]):
    f = line.split()
    sc[(f[1], int(f[3]))][f[2]] = float(f[4])
print('\n### Bulk release scaling\n\nns per object, one run per size:\n')
for p, name in ptrs:
    print('`%s`:\n' % name)
    print(head('objects', alloc))
    for n in sorted({k[1] for k in sc if k[0] == p}):
        print('| %d | %s |' % (n, ' | '.join('%.1f' % sc[(p, n)][a] for a in alloc)))
    print()

print('## Built with `BOOST_DISABLE_THREADS`\n')
print('No `root_ptr` mutex, plain reference counts, no pool mutexes; single-threaded scenarios only.\n')
print('### Allocator alone\n\nTime, ns per operation (speed-up over `pool` in parentheses):\n')
time_table('st', 'raw', ['raw_churn', 'raw_batch'])
for p, name in ptrs:
    print('\n### `%s`\n\nTime, ns per operation (speed-up over `pool` in parentheses):\n' % name)
    time_table('st', p, single)

print('\n## Striped locks (`BOOST_ROOT_PTR_STRIPED_LOCKS`)\n')
print('`boost::root_ptr` only. Time, ns per operation, global lock / striped locks (ratio: above 1.00x the striped build is faster):\n')
print(head('scenario', alloc))
for s in scen:
    print('| %s | %s |' % (s, ' | '.join('%.1f / %.1f (%.2fx)' % (med(('mt', 'root', a, s)), med(('sl', 'root', a, s)),
                                                                    med(('mt', 'root', a, s)) / med(('sl', 'root', a, s)))
                                          for a in alloc)))

def compare(b1, b2, p, rows):
    print(head('scenario', alloc))
    for s in rows:
        if (b1, p, 'pool', s) in na:
            print('| %s | %s |' % (s, ' | '.join('n/a' for a in alloc))); continue
        print('| %s | %s |' % (s, ' | '.join('%.1f / %.1f (%.2fx)' % (med((b1, p, a, s)), med((b2, p, a, s)),
                                                                        med((b1, p, a, s)) / med((b2, p, a, s))) for a in alloc)))

print('\n## Per-thread caches (`BOOST_PAGE_ALLOCATOR_THREAD_CACHE`)\n')
print('Only the page allocators have them; the other columns are the control. Time, ns per operation, '
      'without / with the caches (ratio: above 1.00x the caches are faster):\n')
for p, name in [('raw', 'allocator alone')] + ptrs:
    print('`%s`:\n' % name)
    compare('mt', 'tc', p, ['raw_churn', 'raw_batch'] if p == 'raw' else scen)
    print()
print('`boost::root_ptr` with striped locks, without / with the caches:\n')
compare('sl', 'tcsl', 'root', scen)
print('\nResident memory grown, kB, without / with the caches:\n')
print(head('scenario', alloc))
for p, name in ptrs:
    for s in ['bulk', 'mixed', 'types']:
        print('| %s %s | %s |' % (p, s, ' | '.join('%d / %d' % (statistics.median(m[('mt', p, a, s)]), statistics.median(m[('tc', p, a, s)]))
                                                   for a in alloc)))

print('\n## What thread support costs\n')
print('Time, ns per operation, with thread support / with `BOOST_DISABLE_THREADS` (ratio):\n')
for p, name in [('raw', 'allocator alone')] + ptrs:
    rows = ['raw_churn', 'raw_batch'] if p == 'raw' else single
    print('`%s`:\n' % name)
    print(head('scenario', alloc))
    for s in rows:
        if ('mt', p, 'pool', s) in na:
            print('| %s | %s |' % (s, ' | '.join('n/a' for a in alloc))); continue
        print('| %s | %s |' % (s, ' | '.join('%.1f / %.1f (%.2fx)' % (med(('mt', p, a, s)), med(('st', p, a, s)),
                                                                        med(('mt', p, a, s)) / med(('st', p, a, s))) for a in alloc)))
    print()

import os
if len(sys.argv) > 3 and os.path.exists(sys.argv[3]):
    u = collections.defaultdict(list); um = collections.defaultdict(list); una = set()
    for line in open(sys.argv[3]):
        f = line.split()
        key = tuple(f[:4])
        if f[4] == 'n/a': una.add(key); continue
        u[key].append(float(f[4]))
        if len(f) > 5: um[key].append(int(f[5]))
    umed = lambda k: statistics.median(u[k])
    print('## `boost::shared_node_ptr` against `boost::root_ptr`\n')
    print('A run of its own (bench/run.sh --snode, bench/raw-snode.txt), both pointers interleaved: compare them '
          'with each other only. Time, ns per operation, `shared_node_ptr` / `root_ptr` (ratio: above 1.00x '
          '`shared_node_ptr` is slower):\n')
    for b, title in [('mt', 'With thread support'), ('st', 'With `BOOST_DISABLE_THREADS`'),
                     ('tc', 'With `BOOST_PAGE_ALLOCATOR_THREAD_CACHE`')]:
        print('%s:\n' % title)
        print(head('scenario', alloc))
        for s in scen:
            if b == 'st' and s == 'threads': continue
            cells = []
            for a in alloc:
                if (b, 'snode', a, s) in una:
                    cells.append('n/a / %.1f' % umed((b, 'root', a, s)))
                else:
                    cells.append('%.1f / %.1f (%.2fx)' % (umed((b, 'snode', a, s)), umed((b, 'root', a, s)),
                                                          umed((b, 'snode', a, s)) / umed((b, 'root', a, s))))
            print('| %s | %s |' % (s, ' | '.join(cells)))
        print()
    print('Resident memory grown, kB, `shared_node_ptr` / `root_ptr` (with thread support):\n')
    print(head('scenario', alloc))
    for s in ['bulk', 'mixed', 'types']:
        print('| %s | %s |' % (s, ' | '.join('%d / %d' % (statistics.median(um[('mt', 'snode', a, s)]),
                                                          statistics.median(um[('mt', 'root', a, s)])) for a in alloc)))
    print()
EOF
rm -rf "$TMP"
