#!/usr/bin/env python3
"""Charts of the allocator benchmark, as a PDF.

Reads the raw measurements run.sh keeps (raw-main.txt, raw-scale.txt), takes
the same medians as the Markdown tables, and writes ALLOCATOR_BENCHMARK.pdf.

    usage: bench/plot.py [output.pdf]
"""
import collections
import os
import statistics
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np

DIR = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(DIR, 'ALLOCATOR_BENCHMARK.pdf')

ALLOC = ['pool', 'fast', 'std', 'page_type', 'page_size']
ALLOC_LABEL = {
    'pool': 'boost::pool_allocator (former default)',
    'fast': 'boost::fast_pool_allocator',
    'std': 'std::allocator',
    'page_type': 'page_allocator_by_type',
    'page_size': 'page_allocator_by_size (default)',
}
COLOR = {'pool': '#c0392b', 'fast': '#7f8c8d', 'std': '#2c3e50', 'page_type': '#e67e22', 'page_size': '#27ae60'}
PTRS = [('root', 'boost::root_ptr'), ('unique', 'std::unique_ptr'), ('shared', 'std::shared_ptr')]
PTR_COLOR = {'root': '#8e44ad', 'unique': '#2980b9', 'shared': '#16a085'}
SCEN = ['churn', 'bulk', 'mixed', 'cycles', 'threads', 'types']

# ------------------------------------------------------------------ data
t = collections.defaultdict(list)
m = collections.defaultdict(list)
for line in open(os.path.join(DIR, 'raw-main.txt')):
    f = line.split()
    if f[3] == 'n/a':
        continue
    t[(f[0], f[1], f[2])].append(float(f[3]))
    if len(f) > 4:
        m[(f[0], f[1], f[2])].append(int(f[4]))
T = {k: statistics.median(v) for k, v in t.items()}
M = {k: statistics.median(v) for k, v in m.items()}

scale = collections.defaultdict(dict)
for line in open(os.path.join(DIR, 'raw-scale.txt')):
    p, a, n, ns = line.split()[:4]
    scale[(p, a)][int(n)] = float(ns)

plt.rcParams.update({'font.size': 9, 'axes.titlesize': 11, 'axes.titleweight': 'bold',
                     'axes.spines.top': False, 'axes.spines.right': False,
                     'axes.grid': True, 'grid.alpha': 0.3, 'grid.linestyle': ':'})
A4 = (11.69, 8.27)   # landscape


def legend_below(fig, handles=None, labels=None, ncol=5):
    if handles is None:
        handles = [plt.Rectangle((0, 0), 1, 1, color=COLOR[a]) for a in ALLOC]
        labels = [ALLOC_LABEL[a] for a in ALLOC]
    fig.legend(handles, labels, loc='lower center', ncol=ncol, frameon=False)


def grouped_bars(ax, groups, series, value, color, label=None, log=False, fmt='{:.0f}'):
    """One cluster of bars per group, one bar per series; None values are left blank."""
    x = np.arange(len(groups))
    w = 0.8 / len(series)
    for i, s in enumerate(series):
        vals = [value(g, s) for g in groups]
        pos = x - 0.4 + w * (i + 0.5)
        bars = ax.bar([p for p, v in zip(pos, vals) if v is not None],
                      [v for v in vals if v is not None], w, color=color(s),
                      label=label(s) if label else None)
        for b, v in zip(bars, [v for v in vals if v is not None]):
            ax.annotate(fmt.format(v), (b.get_x() + b.get_width() / 2, b.get_height()),
                        ha='center', va='bottom', fontsize=6, rotation=90, xytext=(0, 2),
                        textcoords='offset points')
    ax.set_xticks(x)
    ax.set_xticklabels(groups)
    if log:
        ax.set_yscale('log')
    ax.margins(y=0.25)


def same_y(axes, log=False):
    """One y range for every chart on a page: from the lowest to the highest bar of any of them,
    with headroom for the value labels (a factor on a log axis, a fraction on a linear one)."""
    axes = list(axes)
    tops, bottoms = [], []
    for ax in axes:
        heights = [b.get_height() for b in ax.patches if b.get_height() > 0]
        if heights:
            tops.append(max(heights))
            bottoms.append(min(heights))
    if log:
        lo, hi = min(bottoms) / 2, max(tops) * 8
    else:
        lo, hi = 0, max(tops) * 1.18
    for ax in axes:
        ax.set_ylim(lo, hi)
        ax.tick_params(labelleft=True)


def short(v):
    return '{:,.0f}'.format(v) if v >= 100 else '{:.1f}'.format(v)


with PdfPages(OUT) as pdf:
    # --------------------------------------------------------- 1. title
    fig = plt.figure(figsize=A4)
    fig.text(0.06, 0.88, 'Node allocator benchmark', fontsize=24, weight='bold')
    fig.text(0.06, 0.83, 'Page allocators vs. the former default pool, for boost::root_ptr, std::unique_ptr '
             'and std::shared_ptr', fontsize=13)
    body = [
        'Measured 2026-09-27 on an Intel Core i7-4700HQ (4 cores / 8 threads), Linux 5.15,',
        'clang 23 -O2, libstdc++ 12, glibc 2.35. Median of 5 runs; every (pointer, allocator,',
        'scenario) triple runs in a fresh process. Full tables and method: ALLOCATOR_BENCHMARK.md.',
        '',
        'Findings',
        '',
        '  - boost::pool_allocator, the former default, is quadratic on release: its deallocate() keeps the free',
        '    list sorted (ordered_free), so releasing 200,000 objects costs 253-436 µs per object.',
        '    Every other allocator stays flat, at 40-160 ns per object.',
        '  - page_allocator_by_size is the fastest, or within 5% of it, in every single-threaded',
        '    scenario, except churn with unique_ptr and shared_ptr (std::allocator 12-17% faster).',
        '    It uses 9-18% less memory than boost::pool_allocator. It is the default node allocator',
        '    since 2026-09-27.',
        '  - page_allocator_by_type costs 2.6-4x the time and 1.6-1.9 MB more memory than by_size',
        '    with many sparse types: each type fills a 64 KiB page of its own.',
        '  - With four threads, std::allocator is 18-38x faster than any pool for unique_ptr and',
        '    shared_ptr; for root_ptr the global mutex narrows the gap to 1.4x.',
        '  - root_ptr costs about 3x unique_ptr in time and 1.8x in memory; it alone reclaims cycles.',
        '',
        'Pages',
        '',
        '  2  Bulk release scaling      3  Time per scenario      4  Resident memory',
        '  5  Pointer types compared    6  Threads',
    ]
    fig.text(0.06, 0.76, '\n'.join(body), fontsize=11, va='top', family='monospace')
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 2. scaling
    fig, axes = plt.subplots(1, 3, figsize=A4, sharey=True)
    for ax, (p, name) in zip(axes, PTRS):
        for a in ALLOC:
            pts = sorted(scale[(p, a)].items())
            ax.plot([n for n, _ in pts], [v for _, v in pts], marker='o', ms=4, color=COLOR[a],
                    lw=2 if a in ('pool', 'page_size') else 1.2)
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_title(name)
        ax.set_xlabel('objects released together')
        n = sorted(scale[(p, 'pool')])
        ax.set_xticks(n)
        ax.set_xticklabels(['{:,}'.format(v) for v in n], rotation=30)
        ax.minorticks_off()
    axes[0].set_ylabel('ns per object (log)')
    fig.suptitle('Bulk release scaling: boost::pool_allocator grows with the number of objects, '
                 'the others stay flat', fontsize=13, weight='bold')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.06, 1, 0.94))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 3. time per scenario
    fig, axes = plt.subplots(2, 2, figsize=A4, sharey=True)
    ax = axes[0][0]
    grouped_bars(ax, ['raw_churn', 'raw_batch'], ALLOC, lambda g, a: T.get(('raw', a, g)),
                 lambda a: COLOR[a], log=True, fmt='{:.0f}')
    ax.set_title('Allocator alone (no pointer)')
    for ax, (p, name) in zip([axes[0][1], axes[1][0], axes[1][1]], PTRS):
        groups = [s for s in SCEN if s != 'threads' and (p == 'root' or s != 'cycles')]
        grouped_bars(ax, groups, ALLOC, lambda g, a: T.get((p, a, g)), lambda a: COLOR[a], log=True)
        ax.set_title(name)
    for row in axes:
        row[0].set_ylabel('ns per operation (log)')
    same_y(axes.flat, log=True)
    fig.suptitle('Time per operation, single-threaded (lower is better)', fontsize=13, weight='bold')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.05, 1, 0.95))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 4. memory
    fig, axes = plt.subplots(1, 3, figsize=A4, sharey=True)
    for ax, (p, name) in zip(axes, PTRS):
        grouped_bars(ax, ['bulk', 'mixed', 'types'], ALLOC,
                     lambda g, a: M[(p, a, g)] / 1024.0, lambda a: COLOR[a], fmt='{:.1f}')
        ax.set_title(name)
    axes[0].set_ylabel('resident memory grown, MB')
    same_y(axes)
    fig.suptitle('Resident memory while the objects are live (lower is better)\n'
                 'bulk: 100,000 x 48 B    mixed: 50,000 each of 16, 48 and 200 B    '
                 'types: 32 types x 100 objects', fontsize=12, weight='bold')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.06, 1, 0.9))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 5. pointer types compared
    fig, axes = plt.subplots(1, 2, figsize=A4, sharey=True)
    handles = [plt.Rectangle((0, 0), 1, 1, color=PTR_COLOR[p]) for p, _ in PTRS]
    for ax, a in zip(axes, ['page_size', 'std']):
        groups = ['churn', 'bulk', 'mixed', 'cycles', 'types']
        grouped_bars(ax, groups, [p for p, _ in PTRS], lambda g, p: T.get((p, a, g)),
                     lambda p: PTR_COLOR[p], fmt='{:.0f}')
        ax.set_title('with ' + ALLOC_LABEL[a])
    axes[0].set_ylabel('ns per operation')
    same_y(axes)
    fig.suptitle('Pointer types compared (cycles: root_ptr only - a unique_ptr cannot form one, '
                 'a shared_ptr cycle is never freed)', fontsize=12, weight='bold')
    legend_below(fig, handles, [n for _, n in PTRS], ncol=3)
    fig.tight_layout(rect=(0, 0.06, 1, 0.94))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 6. threads
    fig, ax = plt.subplots(figsize=A4)
    grouped_bars(ax, [n for _, n in PTRS], ALLOC,
                 lambda g, a: T.get(([p for p, n in PTRS if n == g][0], a, 'threads')),
                 lambda a: COLOR[a], log=True, fmt='{:.0f}')
    ax.set_ylabel('ns per operation, wall clock / all operations (log)')
    fig.suptitle('4 threads, each creating and dropping 250,000 objects (lower is better)',
                 fontsize=13, weight='bold', y=0.97)
    fig.text(0.5, 0.915, "glibc's per-thread caches take no lock; each page pool has one mutex; "
             'every root_ptr operation takes a global mutex', ha='center', fontsize=10, style='italic')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.06, 1, 0.9))
    pdf.savefig(fig)
    plt.close(fig)

    info = pdf.infodict()
    info['Title'] = 'Node allocator benchmark'
    info['Subject'] = 'Page allocators vs. the former default pool for root_ptr, unique_ptr and shared_ptr'
    info['Author'] = 'Services Informatiques Fornux'

print(OUT)
