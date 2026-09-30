#!/bin/bash
# Measures bench/allocbench.cpp over the whole matrix - every build (macro
# combination) x pointer x allocator x scenario - in ONE run, then writes the
# report (bench/report.py: ALLOCATOR_BENCHMARK.md and ALLOCATOR_BENCHMARK.pdf).
#
# Builds: the macros that change what the benchmark runs.
#   BOOST_DISABLE_THREADS               no root_ptr lock, plain counts, no pool mutexes
#   BOOST_ROOT_PTR_STRIPED_LOCKS        address-striped locks for root_ptr (threads only)
#   BOOST_PAGE_ALLOCATOR_THREAD_CACHE   per-thread free-block caches (threads only)
#   BOOST_ZEROIZATION                   every node is cleared when it is released
# Striped locks and thread caches are inactive without threads, so there are
# 2 x 2 x 2 = 8 builds with threads and 2 without: 10. (BOOST_NO_EXCEPTIONS only
# changes root_ptr's static and dynamic cast constructors, which no scenario
# uses.)
#
# Pointers: root (boost::root_ptr) and snode (boost::shared_node_ptr) run in
# every build. unique, shared and raw (the allocator alone) touch no node and no
# root_ptr lock, so striped locks and zeroization cannot change their code: they
# run in the three builds that can differ (mt, st, mt_tc) and the report carries
# those values over to the equivalent builds, marked as such.
#
# Every measurement runs in a fresh process and waits for a cool CPU. Within a
# repeat the loops go scenario > allocator > build > pointer, so drift over the
# hours of a run hits every build and pointer alike.
#
#   usage: bench/run.sh [repeat]      measure (default 5 repeats), then report
#          bench/run.sh --report      report from the kept measurements only
#   BENCH_QUICK=1  every scenario at a tiny size: checks the pipeline in minutes
#                  (into raw-quick*.txt; the numbers mean nothing)
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -n "$BENCH_QUICK" ]; then SUFFIX=quick; else SUFFIX=matrix; fi
RAW="$DIR/raw-$SUFFIX.txt"
SCALE="$DIR/raw-$SUFFIX-scale.txt"
META="$DIR/raw-$SUFFIX-meta.txt"

if [ "$1" = --report ]; then
  exec python3 "$DIR/report.py" "$RAW" "$SCALE" "$META"
fi
REPEAT="${1:-5}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# This machine throttles (Intel powerclamp idle injection, 'kidle_inj' threads)
# under the benchmark's own four-thread load, which made a whole run about 3x
# slower in places. Wait while any kidle_inj thread exists or the package is at
# BENCH_MAX_TEMP (millidegrees C, default 70000) or hotter.
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

# build name -> defines
declare -A DEFS=(
  [mt]=""
  [mt_sl]="-DBOOST_ROOT_PTR_STRIPED_LOCKS"
  [mt_tc]="-DBOOST_PAGE_ALLOCATOR_THREAD_CACHE"
  [mt_sl_tc]="-DBOOST_ROOT_PTR_STRIPED_LOCKS -DBOOST_PAGE_ALLOCATOR_THREAD_CACHE"
  [mt_z]="-DBOOST_ZEROIZATION"
  [mt_sl_z]="-DBOOST_ROOT_PTR_STRIPED_LOCKS -DBOOST_ZEROIZATION"
  [mt_tc_z]="-DBOOST_PAGE_ALLOCATOR_THREAD_CACHE -DBOOST_ZEROIZATION"
  [mt_sl_tc_z]="-DBOOST_ROOT_PTR_STRIPED_LOCKS -DBOOST_PAGE_ALLOCATOR_THREAD_CACHE -DBOOST_ZEROIZATION"
  [st]="-DBOOST_DISABLE_THREADS"
  [st_z]="-DBOOST_DISABLE_THREADS -DBOOST_ZEROIZATION"
)
BUILDS="mt mt_sl mt_tc mt_sl_tc mt_z mt_sl_z mt_tc_z mt_sl_tc_z st st_z"
ALLOCATORS="pool fast std page_type page_size"
SCENARIOS="raw_churn raw_batch churn bulk mixed cycles threads types"
SIZES="12500 25000 50000 100000 200000"

# the pointers measured in build $1 for scenario $2
pointers() {
  case $2 in raw_*) case $1 in mt|st|mt_tc) echo raw ;; esac; return ;; esac
  if [ "$2" = cycles ]; then echo root; return; fi
  case $1 in mt|st|mt_tc) echo root snode unique shared ;; *) echo root snode ;; esac
}

FLAGS="-std=c++20 -O2 -DNDEBUG -DBOOST_ERROR_CODE_HEADER_ONLY -isystem /opt/fornux/superset/usr/include"
for b in $BUILDS; do
  clang++ $FLAGS ${DEFS[$b]} "$DIR/allocbench.cpp" -o "$TMP/$b" -lboost_thread -lpthread &
done
wait
for b in $BUILDS; do [ -x "$TMP/$b" ] || { echo "build $b failed" >&2; exit 1; }; done

QN=""
[ -n "$BENCH_QUICK" ] && QN=2000
rm -f "$RAW" "$SCALE"
{
  echo "start $(date -Iseconds)"
  echo "load $(cut -d' ' -f1-3 /proc/loadavg)"
  echo "repeat $REPEAT"
  echo "cpu $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | sed 's/^ *//')"
  echo "cores $(nproc)"
  echo "kernel $(uname -r)"
  echo "compiler $(clang++ --version | head -1)"
  echo "glibc $(ldd --version | head -1 | awk '{print $NF}')"
  echo "flags $FLAGS"
  for b in $BUILDS; do echo "build $b ${DEFS[$b]}"; done
} > "$META"

for r in $(seq 1 "$REPEAT"); do
  echo "repeat $r/$REPEAT $(date +%T)" >&2
  for s in $SCENARIOS; do
    for a in $ALLOCATORS; do
      for b in $BUILDS; do
        case $b in st*) [ "$s" = threads ] && continue ;; esac
        for p in $(pointers $b $s); do
          prog=$p; [ $p = raw ] && prog=root
          echo "$b $p $a $s $(measure "$TMP/$b" $prog "$a" "$s" $QN)" >> "$RAW"
        done
      done
    done
  done
done

# bulk release by object count: one run per size, default build
for n in $SIZES; do
  [ -n "$BENCH_QUICK" ] && n=$((n / 50))
  for p in root snode unique shared; do
    for a in $ALLOCATORS; do
      echo "mt $p $a $n $(measure "$TMP/mt" "$p" "$a" bulk "$n")" >> "$SCALE"
    done
  done
done
{ echo "end $(date -Iseconds)"; echo "endload $(cut -d' ' -f1-3 /proc/loadavg)"; } >> "$META"

python3 "$DIR/report.py" "$RAW" "$SCALE" "$META"
