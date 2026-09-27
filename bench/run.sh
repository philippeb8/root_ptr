#!/bin/bash
# Builds bench/allocbench.cpp and prints, as Markdown:
#   1. the allocator alone (raw_churn, raw_batch), which involves no pointer;
#   2. per pointer type (root_ptr, unique_ptr, shared_ptr), the median of REPEAT
#      runs of every scenario for every allocator, time and memory;
#   3. the pointer types side by side, per allocator;
#   4. how bulk release scales with the number of objects (one run per size).
# Every (pointer, allocator, scenario) triple runs in a fresh process. The raw
# measurements are kept in bench/raw-main.txt and bench/raw-scale.txt;
# "bench/run.sh --tables" reprints the tables from them without measuring.
#   usage: bench/run.sh [repeat | --tables]
set -e
REPEAT="${1:-5}"
DIR="$(cd "$(dirname "$0")" && pwd)"
MAIN="$DIR/raw-main.txt"
SCALE="$DIR/raw-scale.txt"
TMP="$(mktemp -d)"
BIN="$TMP/allocbench"
if [ "$REPEAT" != --tables ]; then
rm -f "$MAIN" "$SCALE"
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    "$DIR/allocbench.cpp" -o "$BIN" -lboost_thread -lpthread

POINTERS="root unique shared"
ALLOCATORS="pool fast std page_type page_size"
RAW="raw_churn raw_batch"
SCENARIOS="churn bulk mixed cycles threads types"
SIZES="12500 25000 50000 100000 200000"

for r in $(seq 1 "$REPEAT"); do
  for s in $RAW; do
    for a in $ALLOCATORS; do        # interleaved, so drift hits every allocator alike
      echo "raw $a $s $("$BIN" root "$a" "$s")" >> "$MAIN"
    done
  done
  for p in $POINTERS; do
    for s in $SCENARIOS; do
      for a in $ALLOCATORS; do
        echo "$p $a $s $("$BIN" "$p" "$a" "$s")" >> "$MAIN"
      done
    done
  done
done
for p in $POINTERS; do
  for n in $SIZES; do
    for a in $ALLOCATORS; do
      echo "$p $a $n $("$BIN" "$p" "$a" bulk "$n")" >> "$SCALE"
    done
  done
done
fi

python3 - "$MAIN" "$SCALE" <<'EOF'
import sys, statistics, collections
alloc = ['pool', 'fast', 'std', 'page_type', 'page_size']
ptrs = [('root', 'boost::root_ptr'), ('unique', 'std::unique_ptr'), ('shared', 'std::shared_ptr')]
t = collections.defaultdict(list); m = collections.defaultdict(list); na = set()
for line in open(sys.argv[1]):
    f = line.split()
    key = (f[0], f[1], f[2])
    if f[3] == 'n/a': na.add(key); continue
    t[key].append(float(f[3]))
    if len(f) > 4: m[key].append(int(f[4]))
med = lambda k: statistics.median(t[k])
head = lambda first, cols: '| ' + first + ' | ' + ' | '.join(cols) + ' |\n' + '|---' * (len(cols) + 1) + '|'

def time_table(p, scenarios):
    print(head('scenario', alloc))
    for s in scenarios:
        if (p, 'pool', s) in na:
            print('| %s | %s |' % (s, ' | '.join('n/a' for a in alloc))); continue
        base = med((p, 'pool', s))
        print('| %s | %s |' % (s, ' | '.join('%.1f (%.2fx)' % (med((p, a, s)), base / med((p, a, s))) for a in alloc)))

print('### Allocator alone\n\nTime, ns per operation (speed-up over `pool` in parentheses):\n')
time_table('raw', ['raw_churn', 'raw_batch'])
scen = ['churn', 'bulk', 'mixed', 'cycles', 'threads', 'types']
for p, name in ptrs:
    print('\n### `%s`\n\nTime, ns per operation (speed-up over `pool` in parentheses):\n' % name)
    time_table(p, scen)
    print('\nResident memory grown while the objects are live, kB:\n')
    print(head('scenario', alloc))
    for s in ['bulk', 'mixed', 'types']:
        print('| %s | %s |' % (s, ' | '.join('%d' % statistics.median(m[(p, a, s)]) for a in alloc)))

print('\n### Pointer types side by side\n\nTime, ns per operation, `root_ptr` / `unique_ptr` / `shared_ptr`:\n')
print(head('scenario', alloc))
for s in scen:
    cells = []
    for a in alloc:
        cells.append(' / '.join('n/a' if (p, a, s) in na else '%.1f' % med((p, a, s)) for p, _ in ptrs))
    print('| %s | %s |' % (s, ' | '.join(cells)))
print('\nResident memory grown, kB, `root_ptr` / `unique_ptr` / `shared_ptr`:\n')
print(head('scenario', alloc))
for s in ['bulk', 'mixed', 'types']:
    print('| %s | %s |' % (s, ' | '.join(' / '.join('%d' % statistics.median(m[(p, a, s)]) for p, _ in ptrs) for a in alloc)))

sc = collections.defaultdict(dict)
for line in open(sys.argv[2]):
    f = line.split()
    sc[(f[0], int(f[2]))][f[1]] = float(f[3])
print('\n### Bulk release scaling\n\nns per object, one run per size:\n')
for p, name in ptrs:
    print('`%s`:\n' % name)
    print(head('objects', alloc))
    for n in sorted({k[1] for k in sc if k[0] == p}):
        print('| %d | %s |' % (n, ' | '.join('%.1f' % sc[(p, n)][a] for a in alloc)))
    print()
EOF
rm -rf "$TMP"
