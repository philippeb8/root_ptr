#!/bin/bash
# Builds bench/allocbench.cpp and prints, as Markdown:
#   1. the median of REPEAT runs of every scenario for every allocator;
#   2. how bulk release scales with the number of nodes (one run per size).
# Every (allocator, scenario) pair runs in a fresh process.
#   usage: bench/run.sh [repeat]
set -e
REPEAT="${1:-5}"
DIR="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"
BIN="$TMP/allocbench"
clang++ -std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include \
    "$DIR/allocbench.cpp" -o "$BIN" -lboost_thread -lpthread

ALLOCATORS="pool fast std page_type page_size"
SCENARIOS="raw_churn raw_batch root_churn bulk mixed cycles threads types"
SIZES="12500 25000 50000 100000 200000"

for r in $(seq 1 "$REPEAT"); do
  for s in $SCENARIOS; do
    for a in $ALLOCATORS; do        # interleaved, so drift hits every allocator alike
      echo "$a $s $("$BIN" "$a" "$s")" >> "$TMP/main"
    done
  done
done
for n in $SIZES; do
  for a in $ALLOCATORS; do
    echo "$a $n $("$BIN" "$a" bulk "$n")" >> "$TMP/scale"
  done
done

python3 - "$TMP/main" "$TMP/scale" <<'EOF'
import sys, statistics, collections
alloc = ['pool', 'fast', 'std', 'page_type', 'page_size']
t = collections.defaultdict(list); m = collections.defaultdict(list)
for line in open(sys.argv[1]):
    f = line.split()
    t[(f[0], f[1])].append(float(f[2]))
    if len(f) > 3: m[(f[0], f[1])].append(int(f[3]))
head = lambda first: '| ' + first + ' | ' + ' | '.join(alloc) + ' |\n' + '|---' * (len(alloc) + 1) + '|'
print('Time, ns per operation (speed-up over `pool` in parentheses):\n')
print(head('scenario'))
for s in ['raw_churn', 'raw_batch', 'root_churn', 'bulk', 'mixed', 'cycles', 'threads', 'types']:
    base = statistics.median(t[('pool', s)])
    print('| %s | %s |' % (s, ' | '.join('%.1f (%.2fx)' % (statistics.median(t[(a, s)]),
                                                            base / statistics.median(t[(a, s)])) for a in alloc)))
print('\nResident memory grown while the nodes are live, kB:\n')
print(head('scenario'))
for s in ['bulk', 'mixed', 'types']:
    print('| %s | %s |' % (s, ' | '.join('%d' % statistics.median(m[(a, s)]) for a in alloc)))
print('\nBulk release scaling, ns per node (one run per size):\n')
print(head('nodes'))
sc = collections.defaultdict(dict)
for line in open(sys.argv[2]):
    f = line.split()
    sc[int(f[1])][f[0]] = float(f[2])
for n in sorted(sc):
    print('| %d | %s |' % (n, ' | '.join('%.1f' % sc[n][a] for a in alloc)))
EOF
rm -rf "$TMP"
