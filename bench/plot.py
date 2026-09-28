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
# raw-main.txt: <build> <pointer> <allocator> <scenario> <ns> [<kB>]; build is
# mt (thread support, the default), st (BOOST_DISABLE_THREADS), sl (striped
# locks), tc (per-thread caches) or tcsl (both)
for line in open(os.path.join(DIR, 'raw-main.txt')):
    f = line.split()
    if f[4] == 'n/a':
        continue
    t[tuple(f[:4])].append(float(f[4]))
    if len(f) > 5:
        m[tuple(f[:4])].append(int(f[5]))
TB = {k: statistics.median(v) for k, v in t.items()}
MB = {k: statistics.median(v) for k, v in m.items()}
# the existing pages show the default build
T = {k[1:]: v for k, v in TB.items() if k[0] == 'mt'}
M = {k[1:]: v for k, v in MB.items() if k[0] == 'mt'}
ST = {k[1:]: v for k, v in TB.items() if k[0] == 'st'}
SL = {k[1:]: v for k, v in TB.items() if k[0] == 'sl'}
TC = {k[1:]: v for k, v in TB.items() if k[0] == 'tc'}
TCSL = {k[1:]: v for k, v in TB.items() if k[0] == 'tcsl'}

scale = collections.defaultdict(dict)
for line in open(os.path.join(DIR, 'raw-scale.txt')):
    b, p, a, n, ns = line.split()[:5]
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
        'clang 23 -O2, libstdc++ 12, glibc 2.35. Median of 5 runs; every combination runs in a',
        'fresh process. Five builds: with thread support (default), BOOST_DISABLE_THREADS,',
        'BOOST_ROOT_PTR_STRIPED_LOCKS, BOOST_PAGE_ALLOCATOR_THREAD_CACHE, and the last two',
        'together. Full tables and method: ALLOCATOR_BENCHMARK.md.',
        '',
        'Findings',
        '',
        '  - boost::pool_allocator, the former default, is quadratic on release (ordered_free):',
        '    releasing 200,000 objects costs 229-438 us per object; the others stay at 39-171 ns.',
        '  - The page allocators are fastest on bulk, mixed and cycle release; page_allocator_by_size,',
        '    now the default, uses 9-19% less memory than boost::pool_allocator.',
        '  - Per-thread caches (opt-in): 4 threads go 157 -> 1.9 ns per unique_ptr and 149 -> 2.4 ns',
        '    per shared_ptr (std::allocator: 4.1 / 4.8); striped root_ptr 289 -> 27 ns; churn 2.3-3.1x.',
        '  - Thread support is root_ptr\'s largest cost: without it, churn drops 56.2 -> 9.4 ns',
        '    (6.0x), cycles 120 -> 46 ns per node.',
        '  - Striped locks: with 4 threads and std::allocator, root_ptr goes 271 -> 30 ns (9.0x),',
        '    but single-threaded code is 1.33-3.35x slower. Off by default.',
        '  - page_allocator_by_type costs 2.4-4.9x the time and 1.6-1.8 MB more memory than by_size',
        '    with many sparse types: each type fills a 64 KiB page of its own.',
        '',
        'Pages (with thread support unless noted)',
        '',
        '  2  Bulk release scaling      3  Time per scenario      4  Resident memory',
        '  5  Pointer types compared    6  Threads                7  Without thread support',
        '  8  What thread support costs  9  Striped locks (root_ptr)  10  Per-thread caches',
        ' 11  Four threads, every build',
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
                 'the others stay flat\n(lower is better)', fontsize=13, weight='bold')
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
    fig.suptitle('Time per operation, single-threaded scenarios, built with thread support (lower is better)',
                 fontsize=13, weight='bold')
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
    fig.suptitle('Pointer types compared, time per operation (lower is better)\n'
                 'cycles: root_ptr only - a unique_ptr cannot form one, a shared_ptr cycle is never freed',
                 fontsize=12, weight='bold')
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
    fig.text(0.5, 0.915, "without BOOST_PAGE_ALLOCATOR_THREAD_CACHE: glibc's per-thread caches take no lock; each page pool has one mutex;\n"
             'every root_ptr operation takes a global mutex', ha='center', fontsize=10, style='italic')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.06, 1, 0.9))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 7. without thread support
    fig, axes = plt.subplots(2, 2, figsize=A4, sharey=True)
    ax = axes[0][0]
    grouped_bars(ax, ['raw_churn', 'raw_batch'], ALLOC, lambda g, a: ST.get(('raw', a, g)),
                 lambda a: COLOR[a], log=True, fmt='{:.0f}')
    ax.set_title('Allocator alone (no pointer)')
    for ax, (p, name) in zip([axes[0][1], axes[1][0], axes[1][1]], PTRS):
        groups = [s for s in SCEN if s != 'threads' and (p == 'root' or s != 'cycles')]
        grouped_bars(ax, groups, ALLOC, lambda g, a: ST.get((p, a, g)), lambda a: COLOR[a], log=True)
        ax.set_title(name)
    for row in axes:
        row[0].set_ylabel('ns per operation (log)')
    same_y(axes.flat, log=True)
    fig.suptitle('Time per operation, built with BOOST_DISABLE_THREADS (lower is better)\n'
                 'no root_ptr mutex, plain reference counts, no pool mutexes', fontsize=12, weight='bold')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.05, 1, 0.92))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 8. what thread support costs
    fig, axes = plt.subplots(2, 2, figsize=A4, sharey=True)
    for ax, (p, name) in zip(axes.flat, [('raw', 'Allocator alone (no pointer)')] + PTRS):
        groups = (['raw_churn', 'raw_batch'] if p == 'raw'
                  else [s for s in SCEN if s != 'threads' and (p == 'root' or s != 'cycles')])
        grouped_bars(ax, groups, ALLOC,
                     lambda g, a: (T[(p, a, g)] / ST[(p, a, g)]) if (p, a, g) in T and (p, a, g) in ST else None,
                     lambda a: COLOR[a], fmt='{:.1f}x')
        ax.axhline(1.0, color='black', lw=0.8)
        ax.set_title(name)
    for row in axes:
        row[0].set_ylabel('time with / without thread support')
    same_y(axes.flat)
    fig.suptitle('What thread support costs: time with thread support divided by time with '
                 'BOOST_DISABLE_THREADS (lower is better)\n1.0x = thread support costs nothing; above 1.0x it makes the code slower',
                 fontsize=12, weight='bold')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.05, 1, 0.92))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 9. striped locks
    fig, ax = plt.subplots(figsize=A4)
    groups = [s for s in SCEN]
    grouped_bars(ax, groups, ALLOC,
                 lambda g, a: (T[('root', a, g)] / SL[('root', a, g)])
                 if ('root', a, g) in T and ('root', a, g) in SL else None,
                 lambda a: COLOR[a], log=True, fmt='{:.2f}x')
    ax.axhline(1.0, color='black', lw=0.8)
    ax.set_ylabel('time with the global lock / time with striped locks (log)')
    fig.suptitle('boost::root_ptr with BOOST_ROOT_PTR_STRIPED_LOCKS (higher is better)\n'
                 'time with the global lock / time with striped locks: above 1.0x the striped locks are faster',
                 fontsize=12, weight='bold', y=0.985)
    fig.text(0.5, 0.915, 'about 8 lock operations per object instead of 3: slower single-threaded, '
             'faster with four threads unless the allocator itself serializes', ha='center',
             fontsize=10, style='italic')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.06, 1, 0.9))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 10. per-thread caches
    fig, axes = plt.subplots(2, 2, figsize=A4, sharey=True)
    for ax, (p, name) in zip(axes.flat, [('raw', 'Allocator alone (no pointer)')] + PTRS):
        groups = ['raw_churn', 'raw_batch'] if p == 'raw' else [s for s in SCEN if p == 'root' or s != 'cycles']
        grouped_bars(ax, groups, ALLOC,
                     lambda g, a: (T[(p, a, g)] / TC[(p, a, g)]) if (p, a, g) in T and (p, a, g) in TC else None,
                     lambda a: COLOR[a], log=True, fmt='{:.2f}x')
        ax.axhline(1.0, color='black', lw=0.8)
        ax.set_title(name)
    for row in axes:
        row[0].set_ylabel('time without / with the caches (log)')
    same_y(axes.flat, log=True)
    fig.suptitle('Per-thread caches (BOOST_PAGE_ALLOCATOR_THREAD_CACHE) (higher is better)\n'
                 'time without / with the caches: above 1.0x the caches are faster\n'
                 'only the page allocators have them; the other allocators are the control',
                 fontsize=12, weight='bold')
    legend_below(fig)
    fig.tight_layout(rect=(0, 0.05, 1, 0.89))
    pdf.savefig(fig)
    plt.close(fig)

    # --------------------------------------------------------- 11. four threads, every build
    fig, ax = plt.subplots(figsize=A4)
    builds = [('mt', T, 'global lock'), ('tc', TC, 'global lock\n+ caches'),
              ('sl', SL, 'striped locks'), ('tcsl', TCSL, 'striped locks\n+ caches')]
    x, ticks, labels, w = 0.0, [], [], 0.38
    for p, name in PTRS:
        start = x
        for b, data, label in builds:
            if (p, 'std', 'threads') not in data:
                continue                    # striped locks: root_ptr only
            for i, a in enumerate(('std', 'page_size')):
                v = data[(p, a, 'threads')]
                bar = ax.bar(x + (i - 0.5) * w, v, w, color=COLOR[a])[0]
                ax.annotate('{:.1f}'.format(v), (bar.get_x() + w / 2, v), ha='center', va='bottom',
                            fontsize=7, xytext=(0, 2), textcoords='offset points')
            ticks.append(x)
            labels.append(label if p == 'root' else {'mt': 'default', 'tc': 'with caches'}[b])
            x += 1
        ax.text((start + x - 1) / 2, -0.16, name, transform=ax.get_xaxis_transform(), ha='center',
                fontsize=11, weight='bold')
        x += 0.7
    ax.set_xticks(ticks)
    ax.set_xticklabels(labels, fontsize=8)
    ax.set_yscale('log')
    ax.margins(y=0.25)
    ax.set_ylabel('ns per operation, wall clock / all operations (log)')
    fig.suptitle('4 threads, each creating and dropping 250,000 objects, in every build (lower is better)',
                 fontsize=13, weight='bold', y=0.97)
    fig.text(0.5, 0.915, 'striped locks apply to root_ptr only; the caches are the page allocators\' '
             '(std::allocator is the control)', ha='center', fontsize=10, style='italic')
    legend_below(fig, [plt.Rectangle((0, 0), 1, 1, color=COLOR[a]) for a in ('std', 'page_size')],
                 [ALLOC_LABEL[a] for a in ('std', 'page_size')], ncol=2)
    fig.tight_layout(rect=(0, 0.06, 1, 0.9))
    pdf.savefig(fig)
    plt.close(fig)

    info = pdf.infodict()
    info['Title'] = 'Node allocator benchmark'
    info['Subject'] = 'Page allocators vs. the former default pool for root_ptr, unique_ptr and shared_ptr'
    info['Author'] = 'Services Informatiques Fornux'

print(OUT)
