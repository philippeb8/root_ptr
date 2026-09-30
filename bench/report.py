#!/usr/bin/env python3
"""Report of the allocator benchmark matrix: ALLOCATOR_BENCHMARK.md and .pdf.

Reads what bench/run.sh measured - every build (macro combination) x pointer x
allocator x scenario, in one run - takes the median of the repeats, and writes
the same matrices as Markdown tables and as PDF pages.

    usage: bench/report.py <raw> <scale> <meta> [--md out.md] [--pdf out.pdf]
"""
import argparse
import collections
import math
import os
import statistics

import matplotlib
matplotlib.use('Agg')
import matplotlib.colors as mcolors
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import numpy as np

DIR = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument('raw')
ap.add_argument('scale')
ap.add_argument('meta')
ap.add_argument('--md', default=os.path.join(DIR, 'ALLOCATOR_BENCHMARK.md'))
ap.add_argument('--pdf', default=os.path.join(DIR, 'ALLOCATOR_BENCHMARK.pdf'))
args = ap.parse_args()

# ------------------------------------------------------------------ axes
BUILDS = ['mt', 'mt_sl', 'mt_tc', 'mt_sl_tc', 'mt_z', 'mt_sl_z', 'mt_tc_z', 'mt_sl_tc_z', 'st', 'st_z']
BUILD_LABEL = {
    'mt': 'threads (default)', 'mt_sl': 'threads + striped', 'mt_tc': 'threads + caches',
    'mt_sl_tc': 'threads + striped + caches', 'mt_z': 'threads + zero',
    'mt_sl_z': 'threads + striped + zero', 'mt_tc_z': 'threads + caches + zero',
    'mt_sl_tc_z': 'threads + striped + caches + zero', 'st': 'no threads', 'st_z': 'no threads + zero',
}
MACROS = [('no threads', 'BOOST_DISABLE_THREADS'), ('striped', 'BOOST_ROOT_PTR_STRIPED_LOCKS'),
          ('caches', 'BOOST_PAGE_ALLOCATOR_THREAD_CACHE'), ('zero', 'BOOST_ZEROIZATION')]
ALLOC = ['pool', 'fast', 'std', 'page_type', 'page_size']
ALLOC_SHORT = {'pool': 'pool', 'fast': 'fast', 'std': 'std', 'page_type': 'page\ntype', 'page_size': 'page\nsize'}
ALLOC_LABEL = {
    'pool': 'boost::pool_allocator', 'fast': 'boost::fast_pool_allocator', 'std': 'std::allocator',
    'page_type': 'boost::page_allocator_by_type', 'page_size': 'boost::page_allocator_by_size (default)',
}
ALLOC_COLOR = {'pool': '#c0392b', 'fast': '#7f8c8d', 'std': '#2c3e50', 'page_type': '#e67e22', 'page_size': '#27ae60'}
PTRS = ['root', 'snode', 'unique', 'shared']
PTR_LABEL = {'root': 'boost::root_ptr', 'snode': 'boost::shared_node_ptr', 'unique': 'std::unique_ptr',
             'shared': 'std::shared_ptr', 'raw': 'allocator alone'}
SCEN = ['churn', 'bulk', 'mixed', 'cycles', 'threads', 'types']
SCEN_TEXT = {
    'raw_churn': 'allocate and free one 72-byte block, 4,000,000 times',
    'raw_batch': 'allocate 100,000 blocks, then free them in order',
    'churn': 'create and drop one 48-byte object, 1,000,000 times',
    'bulk': '100,000 live 48-byte objects, released together',
    'mixed': '50,000 each of 16, 48 and 200 bytes, interleaved, released together',
    'cycles': '25,000 two-node cycles, reclaimed with their proxy (root_ptr only)',
    'threads': '4 threads, each creating and dropping 250,000 objects (wall clock / all operations)',
    'types': '32 types of equal size, 100 live objects of each',
}
MEM_SCEN = ['bulk', 'mixed', 'types']
# unique, shared and the allocator alone use no node and no root_ptr lock:
# striped locks and zeroization do not change their code
NODELESS = ('unique', 'shared', 'raw')


def equivalent(b, p):
    """The build whose measurement stands for (b, p)."""
    if p not in NODELESS:
        return b
    return 'st' if b.startswith('st') else ('mt_tc' if '_tc' in b else 'mt')


# ------------------------------------------------------------------ data
t = collections.defaultdict(list)
m = collections.defaultdict(list)
for line in open(args.raw):
    f = line.split()
    if len(f) < 5 or f[4] == 'n/a':
        continue
    t[tuple(f[:4])].append(float(f[4]))
    if len(f) > 5:
        m[tuple(f[:4])].append(int(f[5]))
T = {k: statistics.median(v) for k, v in t.items()}
M = {k: statistics.median(v) for k, v in m.items()}
REPEATS = max((len(v) for v in t.values()), default=0)

scale = collections.defaultdict(dict)
for line in open(args.scale):
    f = line.split()
    if len(f) >= 5 and f[4] != 'n/a':
        scale[(f[1], f[2])][int(f[3])] = float(f[4])

meta = collections.OrderedDict()
for line in open(args.meta):
    k, _, v = line.rstrip('\n').partition(' ')
    meta.setdefault(k, []).append(v)
mget = lambda k, d='?': meta.get(k, [d])[0]


def val(b, p, a, s, table=None):
    """(value, carried) - carried when an equivalent build's measurement stands in."""
    table = T if table is None else table
    e = equivalent(b, p)
    v = table.get((e, p, a, s))
    return v, e != b


def applies(b, p, s):
    if s == 'threads' and b.startswith('st'):
        return False
    if s == 'cycles' and p != 'root':
        return False
    return True


# ------------------------------------------------------------------ formatting
def fmt_ns(v):
    if v >= 10000:
        return '{:,.0f}'.format(v)
    if v >= 100:
        return '{:.0f}'.format(v)
    if v >= 10:
        return '{:.1f}'.format(v)
    return '{:.2f}'.format(v)


def fmt_x(v):
    return '{:.2f}x'.format(v)


def fmt_mb(v):
    return '{:.1f}'.format(v / 1024.0)


# ------------------------------------------------------------------ PDF
plt.rcParams.update({'font.size': 8, 'axes.titlesize': 10, 'axes.titleweight': 'bold'})
A4 = (11.69, 8.27)   # landscape
MACRO_NOTE = ('threads: built with thread support (the default)   no threads: BOOST_DISABLE_THREADS   '
              'striped: BOOST_ROOT_PTR_STRIPED_LOCKS   caches: BOOST_PAGE_ALLOCATOR_THREAD_CACHE   '
              'zero: BOOST_ZEROIZATION')
CARRIED_NOTE = ('Faded italic cells: unique_ptr, shared_ptr and the allocator alone have no node and no root_ptr '
                'lock - striped locks and zeroization do not change their code - so they show the equivalent '
                "build's measurement.")


def cell_color(cmap, norm, v, carried):
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return (0.93, 0.93, 0.93, 1.0)
    c = np.array(cmap(norm(v)))
    if carried:
        c[:3] = 0.45 * c[:3] + 0.55
    return tuple(c)


def heatmap(ax, rows, row_labels, cols, groups, cell, fmt, norm, cmap, row_breaks=(), fontsize=6.5,
            group_label=None, col_label=None):
    """rows x cols matrix; cell(r, c) -> (value or None, carried, text override or None).
    cols is a flat list; groups is [(label, n_cols)] in order."""
    nr, nc = len(rows), len(cols)
    img = np.ones((nr, nc, 4))
    for i, r in enumerate(rows):
        for j, c in enumerate(cols):
            v, carried, text = cell(r, c)
            img[i, j] = cell_color(cmap, norm, v, carried)
            if text is None:
                text = 'n/a' if v is None else fmt(v)
            dark = v is not None and not carried and sum(img[i, j][:3]) < 1.3
            ax.text(j, i, text, ha='center', va='center', fontsize=fontsize,
                    color='white' if dark else ('#444444' if carried or v is None else 'black'),
                    style='italic' if carried else 'normal')
    ax.imshow(img, aspect='auto', interpolation='nearest')
    ax.set_yticks(range(nr))
    ax.set_yticklabels(row_labels, fontsize=7)
    ax.set_xticks(range(nc))
    ax.set_xticklabels([col_label(c) if col_label else c for c in cols], fontsize=6.5)
    ax.xaxis.tick_top()
    ax.tick_params(length=0)
    for s in ax.spines.values():
        s.set_visible(False)
    x = 0
    for label, n in groups:
        if x:
            ax.axvline(x - 0.5, color='white', lw=4)
        if label:
            # a fixed distance above the column labels, whatever the row height
            ax.annotate(label, (x + n / 2 - 0.5, 1), xycoords=('data', 'axes fraction'),
                        xytext=(0, 24 if col_label else 12), textcoords='offset points',
                        ha='center', va='bottom', fontsize=8.5, weight='bold')
        x += n
    for b in row_breaks:
        ax.axhline(b - 0.5, color='white', lw=4)


def footer(fig, *lines):
    fig.text(0.5, 0.012, '\n'.join(lines), ha='center', va='bottom', fontsize=7, color='#444444', wrap=True)


def title(fig, main, sub=None):
    fig.text(0.5, 0.975, main, ha='center', va='top', fontsize=13, weight='bold')
    if sub:
        fig.text(0.5, 0.94, sub, ha='center', va='top', fontsize=9, style='italic')


def lognorm(values, clip_pct=97):
    vs = sorted(v for v in values if v is not None and v > 0)
    lo = vs[0]
    hi = vs[min(len(vs) - 1, int(len(vs) * clip_pct / 100))]
    if hi <= lo:
        hi = lo * 1.01
    return mcolors.LogNorm(vmin=lo, vmax=hi, clip=True)


def ratio_norm(values, cap=8.0):
    vs = [abs(math.log(v)) for v in values if v is not None and v > 0]
    span = min(max(vs + [math.log(1.1)]), math.log(cap))
    return mcolors.LogNorm(vmin=math.exp(-span), vmax=math.exp(span), clip=True)


TIME_CMAP = plt.get_cmap('RdYlGn_r')     # green = less time
RATIO_CMAP = plt.get_cmap('RdYlGn')      # green = above 1.00x
MEM_CMAP = plt.get_cmap('RdYlGn_r')

BUILD_ROWS = BUILDS
ROW_BREAKS = (4, 8)


def colorbar(fig, norm, cmap, label, rect=(0.35, 0.1, 0.3, 0.015)):
    cax = fig.add_axes(rect)
    cb = fig.colorbar(plt.cm.ScalarMappable(norm=norm, cmap=cmap), cax=cax, orientation='horizontal')
    cb.set_label(label, fontsize=7)
    cb.ax.tick_params(labelsize=6)


pages = []   # (title, page number) for the contents


def time_page(pdf, s):
    cols = [(p, a) for p in PTRS for a in ALLOC]
    cell = lambda b, c: ((val(b, c[0], c[1], s)[0], val(b, c[0], c[1], s)[1], None)
                         if applies(b, c[0], s) else (None, False, 'n/a'))
    values = [cell(b, c)[0] for b in BUILDS for c in cols]
    norm = lognorm(values)
    fig = plt.figure(figsize=A4)
    ax = fig.add_axes((0.17, 0.17, 0.81, 0.67))
    heatmap(ax, BUILDS, [BUILD_LABEL[b] for b in BUILDS], cols, [(PTR_LABEL[p], len(ALLOC)) for p in PTRS],
            cell, fmt_ns, norm, TIME_CMAP, ROW_BREAKS, col_label=lambda c: ALLOC_SHORT[c[1]])
    title(fig, 'Time per operation: %s (lower is better)' % s, SCEN_TEXT[s] + ' - ns per operation, median of %d runs' % REPEATS)
    colorbar(fig, norm, TIME_CMAP, 'ns per operation (log; green = less time)')
    footer(fig, MACRO_NOTE, CARRIED_NOTE)
    pdf.savefig(fig)
    plt.close(fig)


def cycles_raw_page(pdf):
    fig = plt.figure(figsize=A4)
    ax1 = fig.add_axes((0.17, 0.17, 0.25, 0.64))
    cell1 = lambda b, a: (val(b, 'root', a, 'cycles')[0], False, None)
    v1 = [cell1(b, a)[0] for b in BUILDS for a in ALLOC]
    n1 = lognorm(v1, 100)
    heatmap(ax1, BUILDS, [BUILD_LABEL[b] for b in BUILDS], ALLOC, [('cycles: boost::root_ptr', len(ALLOC))],
            cell1, fmt_ns, n1, TIME_CMAP, ROW_BREAKS, col_label=lambda a: ALLOC_SHORT[a])
    ax2 = fig.add_axes((0.5, 0.17, 0.48, 0.64))
    cols = [(s, a) for s in ('raw_churn', 'raw_batch') for a in ALLOC]
    cell2 = lambda b, c: val(b, 'raw', c[1], c[0]) + (None,)
    v2 = [cell2(b, c)[0] for b in BUILDS for c in cols]
    n2 = lognorm(v2, 100)
    heatmap(ax2, BUILDS, ['' for b in BUILDS], cols,
            [('allocator alone: churn', len(ALLOC)), ('allocator alone: batch', len(ALLOC))],
            cell2, fmt_ns, n2, TIME_CMAP, ROW_BREAKS, col_label=lambda c: ALLOC_SHORT[c[1]])
    title(fig, 'Time per operation: cycles, and the allocator alone (lower is better)',
          'cycles: %s, ns per node.\nallocator alone: one node-sized block, no pointer - churn: %s; batch: %s'
          % (SCEN_TEXT['cycles'], SCEN_TEXT['raw_churn'], SCEN_TEXT['raw_batch']))
    colorbar(fig, n1, TIME_CMAP, 'cycles: ns per node (log)', (0.2, 0.1, 0.2, 0.015))
    colorbar(fig, n2, TIME_CMAP, 'allocator alone: ns per operation (log)', (0.6, 0.1, 0.25, 0.015))
    footer(fig, MACRO_NOTE, CARRIED_NOTE)
    pdf.savefig(fig)
    plt.close(fig)


def memory_page(pdf, s):
    cols = [(p, a) for p in PTRS for a in ALLOC]
    cell = lambda b, c: val(b, c[0], c[1], s, M) + (None,)
    values = [cell(b, c)[0] for b in BUILDS for c in cols]
    norm = lognorm(values, 100)
    fig = plt.figure(figsize=A4)
    ax = fig.add_axes((0.17, 0.17, 0.81, 0.67))
    heatmap(ax, BUILDS, [BUILD_LABEL[b] for b in BUILDS], cols, [(PTR_LABEL[p], len(ALLOC)) for p in PTRS],
            cell, fmt_mb, norm, MEM_CMAP, ROW_BREAKS, col_label=lambda c: ALLOC_SHORT[c[1]])
    title(fig, 'Resident memory grown while the objects are live: %s (lower is better)' % s,
          SCEN_TEXT[s] + ' - MB, median of %d runs' % REPEATS)
    colorbar(fig, norm, MEM_CMAP, 'MB (log; green = less memory)')
    footer(fig, MACRO_NOTE, CARRIED_NOTE)
    pdf.savefig(fig)
    plt.close(fig)


# The effect of one setting: pairs of builds that differ by it alone.
EFFECTS = [
    ('turning thread support off (BOOST_DISABLE_THREADS)', 'no threads',
     [('mt', 'st', 'default'), ('mt_z', 'st_z', 'with zero')],
     ['raw', 'root', 'snode', 'unique', 'shared']),
    ('striped locks (BOOST_ROOT_PTR_STRIPED_LOCKS)', 'striped',
     [('mt', 'mt_sl', 'default'), ('mt_tc', 'mt_sl_tc', 'with caches'), ('mt_z', 'mt_sl_z', 'with zero'),
      ('mt_tc_z', 'mt_sl_tc_z', 'with caches + zero')],
     ['root', 'snode']),
    ('per-thread caches (BOOST_PAGE_ALLOCATOR_THREAD_CACHE)', 'caches',
     [('mt', 'mt_tc', 'default'), ('mt_sl', 'mt_sl_tc', 'with striped'), ('mt_z', 'mt_tc_z', 'with zero'),
      ('mt_sl_z', 'mt_sl_tc_z', 'with striped + zero')],
     ['raw', 'root', 'snode', 'unique', 'shared']),
    ('zeroization (BOOST_ZEROIZATION)', 'zero',
     [('mt', 'mt_z', 'default'), ('mt_sl', 'mt_sl_z', 'with striped'), ('mt_tc', 'mt_tc_z', 'with caches'),
      ('mt_sl_tc', 'mt_sl_tc_z', 'with striped + caches'), ('st', 'st_z', 'no threads')],
     ['root', 'snode']),
]


def effect_rows(ptrs):
    rows = []
    for p in ptrs:
        for s in (['raw_churn', 'raw_batch'] if p == 'raw' else SCEN):
            if s == 'cycles' and p != 'root':
                continue
            rows.append((p, s))
    return rows


def effect_cell(pair, p, s, a):
    off, on = pair
    if not (applies(off, p, s) and applies(on, p, s)):
        return None, False, 'n/a'
    v0, c0 = val(off, p, a, s)
    v1, c1 = val(on, p, a, s)
    if v0 is None or v1 is None:
        return None, False, 'n/a'
    if equivalent(off, p) == equivalent(on, p):
        return None, False, 'same'
    return v0 / v1, c0 or c1, None


def effect_page(pdf, name, short, pairs, ptrs):
    cols = [(i, a) for i in range(len(pairs)) for a in ALLOC]
    cell = lambda r, c: effect_cell(pairs[c[0]][:2], r[0], r[1], c[1])
    rows = [r for r in effect_rows(ptrs) if any(cell(r, c)[2] != 'n/a' for c in cols)]
    values = [cell(r, c)[0] for r in rows for c in cols]
    norm = ratio_norm(values)
    fig = plt.figure(figsize=A4)
    height = min(0.64, 0.045 * len(rows) + 0.04)
    ax = fig.add_axes((0.2, 0.84 - height, 0.78, height))
    breaks, last = [], None
    for i, r in enumerate(rows):
        if last is not None and r[0] != last:
            breaks.append(i)
        last = r[0]
    heatmap(ax, rows, ['%s: %s' % (PTR_LABEL[p], s) for p, s in rows], cols,
            [(pr[2], len(ALLOC)) for pr in pairs],
            cell, fmt_x, norm, RATIO_CMAP, breaks, fontsize=6 if len(pairs) > 4 else 6.5,
            col_label=lambda c: ALLOC_SHORT[c[1]])
    fig.text(0.2, 0.84 - height - 0.03, 'Pairs of builds, without -> with the setting:   ' + ';   '.join(
        '%s: %s -> %s' % (pr[2], BUILD_LABEL[pr[0]], BUILD_LABEL[pr[1]]) for pr in pairs),
        fontsize=7, va='top', wrap=True)
    title(fig, 'Effect of %s (higher is better)' % name,
          'time without / time with the setting, each pair of builds differing by it alone: above 1.00x the '
          'setting is faster, below it is slower')
    colorbar(fig, norm, RATIO_CMAP, 'time without / time with (log; green = the setting is faster)')
    footer(fig, MACRO_NOTE, "'same': the pointer's code does not change with this setting. " + CARRIED_NOTE)
    pdf.savefig(fig)
    plt.close(fig)


def best_rows():
    return [('raw', s) for s in ('raw_churn', 'raw_batch')] + effect_rows(PTRS)


def best_cell(b, p, s):
    if not applies(b, p, s):
        return None
    got = [(val(b, p, a, s)[0], a) for a in ALLOC]
    got = [(v, a) for v, a in got if v is not None]
    if not got:
        return None
    v, a = min(got)
    d = val(b, p, 'page_size', s)[0]
    return a, v, (v / d if d else None), val(b, p, a, s)[1]


def best_page(pdf):
    rows = best_rows()
    fig = plt.figure(figsize=A4)
    ax = fig.add_axes((0.2, 0.12, 0.78, 0.72))
    nr, nc = len(rows), len(BUILDS)
    img = np.ones((nr, nc, 4))
    for i, (p, s) in enumerate(rows):
        for j, b in enumerate(BUILDS):
            r = best_cell(b, p, s)
            if r is None:
                img[i, j] = (0.93, 0.93, 0.93, 1)
                ax.text(j, i, 'n/a', ha='center', va='center', fontsize=6, color='#444444')
                continue
            a, v, rel, carried = r
            c = np.array(mcolors.to_rgba(ALLOC_COLOR[a]))
            c[:3] = 0.25 * c[:3] + 0.75 if carried else 0.45 * c[:3] + 0.55
            img[i, j] = c
            ax.text(j, i, '%s %s' % (a.replace('page_', 'p_'), '' if a == 'page_size' else '%.2f' % rel),
                    ha='center', va='center', fontsize=6, style='italic' if carried else 'normal')
    ax.imshow(img, aspect='auto', interpolation='nearest')
    ax.set_yticks(range(nr))
    ax.set_yticklabels(['%s: %s' % (PTR_LABEL[p], s) for p, s in rows], fontsize=6.5)
    ax.set_xticks(range(nc))
    ax.set_xticklabels([BUILD_LABEL[b].replace(' + ', '\n+ ') for b in BUILDS], fontsize=6.5)
    ax.xaxis.tick_top()
    ax.tick_params(length=0)
    for sp in ax.spines.values():
        sp.set_visible(False)
    for j in (4, 8):
        ax.axvline(j - 0.5, color='white', lw=4)
    last = None
    for i, (p, s) in enumerate(rows):
        if last is not None and p != last:
            ax.axhline(i - 0.5, color='white', lw=4)
        last = p
    title(fig, 'Fastest allocator for every build, pointer and scenario',
          'the number: its time / the time of page_allocator_by_size (the default) in the same cell - '
          'lower is better; none shown: the default is the fastest')
    fig.legend([plt.Rectangle((0, 0), 1, 1, color=ALLOC_COLOR[a]) for a in ALLOC], [ALLOC_LABEL[a] for a in ALLOC],
               loc='lower center', ncol=5, frameon=False, bbox_to_anchor=(0.5, 0.06), fontsize=7)
    footer(fig, MACRO_NOTE, 'p_type: page_allocator_by_type.  ' + CARRIED_NOTE)
    pdf.savefig(fig)
    plt.close(fig)


VS_ROOT_SCEN = ['churn', 'bulk', 'mixed', 'threads', 'types']


def vs_root_page(pdf):
    fig = plt.figure(figsize=A4)
    axes = [fig.add_axes((0.17, 0.55, 0.81, 0.3)), fig.add_axes((0.17, 0.16, 0.81, 0.3))]
    cols = [(s, p) for s in VS_ROOT_SCEN for p in ('snode', 'unique', 'shared')]
    allv, cells = [], {}
    for a in ('page_size', 'std'):
        for b in BUILDS:
            for c in cols:
                s, p = c
                if not applies(b, p, s):
                    cells[(a, b, c)] = (None, False, 'n/a')
                    continue
                v, carried = val(b, p, a, s)
                rv = val(b, 'root', a, s)[0]
                cells[(a, b, c)] = (v / rv if v and rv else None, carried, None)
                allv.append(cells[(a, b, c)][0])
    norm = ratio_norm(allv, 64)
    cmap = plt.get_cmap('RdYlGn_r')
    for ax, a in zip(axes, ('page_size', 'std')):
        heatmap(ax, BUILDS, [BUILD_LABEL[b] for b in BUILDS], cols, [(s, 3) for s in VS_ROOT_SCEN],
                lambda b, c, a=a: cells[(a, b, c)], fmt_x, norm, cmap, ROW_BREAKS,
                col_label=lambda c: {'snode': 'shared\nnode', 'unique': 'unique', 'shared': 'shared'}[c[1]])
        box = ax.get_position()
        fig.text(0.02, box.y1 + 0.035, 'with\n%s' % ALLOC_LABEL[a].replace(' (default)', '\n(default)'),
                 fontsize=8.5, weight='bold', va='bottom')
    title(fig, 'The other pointers against boost::root_ptr (lower is better)',
          'time of the pointer / time of boost::root_ptr in the same build, allocator and scenario: '
          'below 1.00x the pointer is faster than root_ptr')
    colorbar(fig, norm, cmap, 'pointer / root_ptr (log; green = faster than root_ptr)', (0.35, 0.095, 0.3, 0.013))
    footer(fig, MACRO_NOTE, CARRIED_NOTE + ' Cycles: only root_ptr reclaims them.')
    pdf.savefig(fig)
    plt.close(fig)


def scaling_page(pdf):
    fig, axes = plt.subplots(1, 4, figsize=A4, sharey=True)
    for ax, p in zip(axes, PTRS):
        for a in ALLOC:
            pts = sorted(scale[(p, a)].items())
            if pts:
                ax.plot([n for n, _ in pts], [v for _, v in pts], marker='o', ms=3, color=ALLOC_COLOR[a],
                        lw=2 if a in ('pool', 'page_size') else 1.2)
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_title(PTR_LABEL[p])
        ax.set_xlabel('objects released together')
        ns = sorted(scale[(p, 'pool')])
        if ns:
            ax.set_xticks(ns)
            ax.set_xticklabels(['{:,}'.format(v) for v in ns], rotation=40, fontsize=6.5)
        ax.minorticks_off()
        ax.grid(alpha=0.3, ls=':')
    axes[0].set_ylabel('ns per object (log)')
    title(fig, 'Bulk release scaling, threads (default) build (lower is better)',
          'ns per object, one run per size: boost::pool_allocator grows with the number of objects, the others stay flat')
    fig.legend([plt.Line2D([], [], color=ALLOC_COLOR[a], marker='o') for a in ALLOC], [ALLOC_LABEL[a] for a in ALLOC],
               loc='lower center', ncol=5, frameon=False, fontsize=7)
    fig.tight_layout(rect=(0, 0.05, 1, 0.92))
    pdf.savefig(fig)
    plt.close(fig)


# ------------------------------------------------------------------ findings
# Written after reading the matrices of the run the report is built from.
# (short line for the title page, full text for the findings page and the Markdown summary)
FINDINGS = [
    ('The default, page_allocator_by_size, is fastest or within 10% in 114 of 140 cells.',
     'The default allocator, page_allocator_by_size, is the fastest or within 10% of the fastest in 114 of the '
     '140 measured (build, pointer, scenario) cells. It loses by a wide margin only with 4 threads and no thread '
     'caches, where every allocation takes the page pool\'s mutex: std::allocator (glibc, per-thread arenas) is '
     '36x faster for unique_ptr (5.0 vs. 182.9 ns), 28x for shared_ptr, 19x for shared_node_ptr, and 1.24x for '
     'root_ptr behind its global lock (313 vs. 389 ns).'),
    ('boost::pool_allocator is quadratic on release: 85-144 us per object at 100,000 objects.',
     'boost::pool_allocator is quadratic when many objects are released: its deallocate keeps the free list '
     'sorted (ordered_free). Releasing 100,000 objects together costs 85-144 us per object for every pointer and '
     'build, and about 2x more each time the count doubles (189-402 us at 200,000). Every other allocator stays '
     'at 41-255 ns in every build. The allocator alone shows it too: a batch costs 70.7 us per block against '
     '28.9 ns.'),
    ('Thread support is root_ptr\'s largest cost: churn 58.0 -> 9.4 ns without it (6.2x).',
     'Thread support (the default) is root_ptr\'s largest single cost: with BOOST_DISABLE_THREADS and '
     'page_allocator_by_size, churn drops from 58.0 to 9.4 ns (6.2x), cycles from 139.6 to 50.9 ns per node '
     '(2.7x) and bulk release from 133.4 to 74.8 ns (1.8x): the global root_ptr mutex, atomic counts and the pool '
     'mutex go away. shared_node_ptr churn drops 2.2x (61.8 to 28.7 ns), unique_ptr 3.2x (18.6 to 5.8 ns, the '
     'page pool\'s mutex); std::allocator itself does not change (16.3 vs. 15.8 ns).'),
    ('Striped locks: root_ptr with 4 threads 10.5x faster, single-threaded 1.5-2.6x slower.',
     'Striped locks (BOOST_ROOT_PTR_STRIPED_LOCKS) trade single-threaded speed for scaling. With 4 threads, '
     'root_ptr goes from 313 to 29.7 ns per object with std::allocator (10.5x), and from 309 to 30.2 ns with '
     'page_allocator_by_size plus thread caches (10.2x); without the caches the page pool\'s mutex caps the gain at '
     '1.36x. Single-threaded every root_ptr scenario is slower: 1.5x to 2.6x (geometric means), e.g. churn '
     '58.0 to 120.2 ns and cycles 139.6 to 451.1 ns. shared_node_ptr takes no root_ptr lock and does not change '
     '(0.99-1.04x).'),
    ('Thread caches: 4 threads 183 -> 3.4 ns per unique_ptr; churn 1.3x-3.1x faster.',
     'Per-thread caches (BOOST_PAGE_ALLOCATOR_THREAD_CACHE) take the page pool\'s mutex off the common path. '
     'With 4 threads and page_allocator_by_size: unique_ptr 182.9 to 3.4 ns, shared_ptr 166.5 to 4.3 ns, '
     'shared_node_ptr 238.9 to 12.4 ns - now level with or faster than std::allocator (5.1, 5.8, 15.2 ns); '
     'root_ptr needs striped locks as well (above). Churn: the allocator alone 3.1x faster (17.1 to 5.5 ns), '
     'unique_ptr 2.9x, shared_ptr 2.7x, shared_node_ptr 1.6x, root_ptr 1.3x. Bulk, mixed, types, cycles and '
     'resident memory do not change (0.99-1.04x).'),
    ('Zeroization (BOOST_ZEROIZATION) costs nothing measurable: 0.97-1.01x.',
     'Zeroization (BOOST_ZEROIZATION) costs nothing measurable: the geometric mean of time without / with it is '
     '0.97-1.01x for every pointer and scenario, and single cells scatter within run-to-run noise (0.72-1.22x). '
     'Clearing a 72-248 byte node is small next to a 50-150 ns release. (Checked separately that the macro is '
     'active: a released node reads back as zeros.)'),
    ('root_ptr vs. unique_ptr / shared_ptr: churn 3.1x / 2.6x with threads, 1.6x / 1.0x without.',
     'root_ptr against the standard pointers (page_allocator_by_size): churn costs 3.1x unique_ptr and 2.6x '
     'shared_ptr with thread support (58.0 vs. 18.6 and 22.7 ns), 1.6x and 1.0x without (9.4 vs. 5.8 and 9.3 ns). '
     'It holds 100,000 objects in 12.5 MB against 7.0 MB (unique_ptr) and 9.3 MB (shared_ptr). It is the only one '
     'of them that reclaims cycles.'),
    ('shared_node_ptr: 1.06-1.15x root_ptr single-threaded, 1.6x-25x faster with 4 threads.',
     'shared_node_ptr (what FCXXSS_SHARED_PTR emits) against root_ptr, both on page_allocator_by_size: '
     'single-threaded with thread support it costs 1.06-1.15x as much (churn 61.8 vs. 58.0 ns) and holds the '
     'same objects in 19-25% more memory (a control block per object on top of the node). With 4 threads it '
     'has no global lock: 1.6x faster (239 vs. 389 ns), 24x with std::allocator and 25x with the thread caches '
     '(12.4 vs. 309 ns). Without thread support it is the slower one: churn 3.1x (28.7 vs. 9.4 ns). A cycle '
     'through it is never released.'),
    ('page_allocator_by_type with 32 sparse types: 2.1x-4.3x the time and +1.6-1.9 MB.',
     'page_allocator_by_type costs more with many sparse types: 32 types of 100 objects each take 2.1x to 4.3x '
     'the time of page_allocator_by_size and 1.6-1.9 MB more memory (3.8-4.2 MB against 1.9-2.3 MB), because '
     'each type fills a 64 KiB page of its own.'),
    ('Noise: the 5 runs of a cell differ by 1.21x (median); under ~10% is not a difference.',
     'Noise: the slowest of a cell\'s 5 runs is 1.21x its fastest at the median (1.36x at the 90th percentile, '
     'up to 4x in the 4-thread scenario), so the medians are reported, and differences below about 10% are '
     'not significant. The run took 2.5 hours on a shared desktop machine.'),
]


CONTENTS = [
    ('Findings', 2),
    ('Setup, builds and how to read the matrices', 3),
    ('Time per operation: churn, bulk, mixed, types, threads', '4-8'),
    ('Time per operation: cycles, and the allocator alone', 9),
    ('Resident memory: bulk, mixed, types', '10-12'),
    ('Effect of each setting: no threads, striped locks, caches, zeroization', '13-16'),
    ('Fastest allocator per build, pointer and scenario', 17),
    ('The other pointers against root_ptr', 18),
    ('Bulk release scaling', 19),
]


def title_page(pdf):
    fig = plt.figure(figsize=A4)
    fig.text(0.06, 0.9, 'Node allocator benchmark: the whole matrix', fontsize=22, weight='bold')
    fig.text(0.06, 0.855, '%d builds (macro combinations) x %d allocators x %d pointer types x %d scenarios, '
             'measured in one run' % (len(BUILDS), len(ALLOC), len(PTRS) + 1, len(SCEN) + 2), fontsize=12)
    lines = ['Measured %s to %s, %s (%s logical CPUs), Linux %s,' % (
                 mget('start')[:16].replace('T', ' '), mget('end', '?')[11:16], mget('cpu'), mget('cores'),
                 mget('kernel')),
             '%s, glibc %s; -O2. Median of %d runs; every measurement in a fresh process,' % (
                 mget('compiler').split(' (')[0], mget('glibc'), REPEATS),
             'after the CPU has cooled. Tables: ALLOCATOR_BENCHMARK.md.', '', 'Findings', '']
    for short, _ in FINDINGS:
        lines.append('  - ' + short)
    lines += ['', 'Pages', '']
    for name, pg in CONTENTS:
        lines.append('  %-6s %s' % (pg, name))
    fig.text(0.06, 0.8, '\n'.join(lines), fontsize=9.5, va='top', family='monospace')
    pdf.savefig(fig)
    plt.close(fig)


def findings_page(pdf):
    import textwrap
    fig = plt.figure(figsize=A4)
    title(fig, 'Findings', 'numbers are medians of %d runs; ns per operation unless noted' % REPEATS)
    lines = []
    for _, full in FINDINGS:
        wrapped = textwrap.wrap(full, 150)
        lines += ['- ' + wrapped[0]] + ['  ' + l for l in wrapped[1:]] + ['']
    fig.text(0.04, 0.9, '\n'.join(lines), fontsize=7.6, va='top', family='monospace')
    pdf.savefig(fig)
    plt.close(fig)


def setup_page(pdf):
    fig = plt.figure(figsize=A4)
    title(fig, 'Setup, builds and how to read the matrices')
    ax = fig.add_axes((0.05, 0.5, 0.9, 0.4))
    ax.axis('off')
    rows = []
    for b in BUILDS:
        on = {'no threads': b.startswith('st'), 'striped': '_sl' in b, 'caches': '_tc' in b, 'zero': '_z' in b}
        ptrs = 'all' if b in ('mt', 'st', 'mt_tc') else 'root_ptr, shared_node_ptr'
        rows.append([BUILD_LABEL[b]] + ['yes' if on[k] else '' for k, _ in MACROS] + [ptrs])
    tb = ax.table(cellText=rows, colLabels=['build'] + [k for k, _ in MACROS] + ['pointers measured'],
                  loc='upper center', cellLoc='center', colLoc='center')
    tb.auto_set_font_size(False)
    tb.set_fontsize(7)
    tb.scale(1, 1.35)
    text = [
        'Macros. BOOST_DISABLE_THREADS: no root_ptr lock, plain reference counts, no pool mutexes.',
        'BOOST_ROOT_PTR_STRIPED_LOCKS: 1024 address-striped spin locks instead of root_ptr\'s global mutex.',
        'BOOST_PAGE_ALLOCATOR_THREAD_CACHE: each thread keeps up to 16 KiB of free blocks of every page pool.',
        'BOOST_ZEROIZATION: every node is cleared (memset) before its memory is released.',
        'Striped locks and thread caches are inactive without threads, hence 8 + 2 builds. BOOST_NO_EXCEPTIONS only',
        'changes root_ptr\'s static and dynamic cast constructors, which no scenario uses.',
        '',
        'Pointers. root_ptr: root_ptr<T>(x, new node<T, A<T>>(..)); shared_node_ptr (FCXXSS_SHARED_PTR): a',
        'std::shared_ptr owning the same node; shared_ptr: std::allocate_shared; unique_ptr: an allocate_unique',
        'with a deleter freeing through the same allocator; allocator alone: one node-sized block, no pointer.',
        'unique_ptr, shared_ptr and the allocator alone are measured in the builds where their code can differ',
        '(threads, threads + caches, no threads); faded italic cells carry those values to the equivalent builds.',
        '',
        'Reading. Time and memory pages: rows are builds, columns allocators grouped by pointer; green = less.',
        'Effect pages: time without / time with one setting; green (above 1.00x) = the setting is faster.',
        'Every page says whether lower or higher is better. Colours are per page; a log scale throughout.',
    ]
    fig.text(0.05, 0.47, '\n'.join(text), fontsize=8.5, va='top', family='monospace')
    pdf.savefig(fig)
    plt.close(fig)


with PdfPages(args.pdf) as pdf:
    title_page(pdf)
    findings_page(pdf)
    setup_page(pdf)
    for s in ['churn', 'bulk', 'mixed', 'types', 'threads']:
        time_page(pdf, s)
    cycles_raw_page(pdf)
    for s in MEM_SCEN:
        memory_page(pdf, s)
    for name, short, pairs, ptrs in EFFECTS:
        effect_page(pdf, name, short, pairs, ptrs)
    best_page(pdf)
    vs_root_page(pdf)
    scaling_page(pdf)
    info = pdf.infodict()
    info['Title'] = 'Node allocator benchmark: the whole matrix'
    info['Author'] = 'Services Informatiques Fornux'


# ------------------------------------------------------------------ Markdown
out = []
w = out.append


def md_table(header, rows):
    w('| ' + ' | '.join(header) + ' |')
    w('|' + '---|' * len(header))
    for r in rows:
        w('| ' + ' | '.join(r) + ' |')
    w('')


def md_cell(v, carried, fmt, text=None):
    if text is not None:
        return text
    if v is None:
        return 'n/a'
    return ('*%s*' % fmt(v)) if carried else fmt(v)


w('# Node allocator benchmark: the whole matrix\n')
w('Every build (macro combination) x allocator x pointer type x scenario, measured in one run with '
  '`bench/allocbench.cpp` and `bench/run.sh`; this file and `ALLOCATOR_BENCHMARK.pdf` (the same matrices '
  'as colour pages) are written by `bench/report.py`. The previous report (2026-09-27/28, measured in '
  'separate runs) is kept in `bench/archive-2026-09-27/`.\n')
w('## Summary\n')
for _, full in FINDINGS:
    w('- ' + full)
w('')
w('## Setup\n')
md_table(['', ''], [
    ['Measured', '%s to %s' % (mget('start')[:16].replace('T', ' '), mget('end', '?')[:16].replace('T', ' '))],
    ['CPU', '%s, %s logical CPUs' % (mget('cpu'), mget('cores'))],
    ['OS', 'Linux %s' % mget('kernel')],
    ['Compiler', '%s, glibc %s' % (mget('compiler').split(' (')[0], mget('glibc'))],
    ['Flags', '`%s`' % mget('flags')],
    ['Load', 'load average %s at the start, %s at the end' % (mget('load'), mget('endload', '?'))],
    ['Method', 'median of %d runs; every measurement in a fresh process, after the CPU has cooled '
               '(this machine throttles under the four-thread load); loops ordered scenario > allocator > '
               'build > pointer so drift hits every build alike' % REPEATS],
])
w('### Builds\n')
w('The macros that change what the benchmark runs. Striped locks and thread caches are inactive without '
  'threads, hence 8 builds with threads and 2 without. `BOOST_NO_EXCEPTIONS` only changes `root_ptr`\'s '
  'static and dynamic cast constructors, which no scenario uses.\n')
md_table(['build'] + ['`%s`' % k for _, k in MACROS] + ['pointers measured'],
         [[BUILD_LABEL[b]] + ['yes' if on else '' for on in
                              (b.startswith('st'), '_sl' in b, '_tc' in b, '_z' in b)] +
          ['all' if b in ('mt', 'st', 'mt_tc') else '`root_ptr`, `shared_node_ptr`'] for b in BUILDS])
md_table(['macro', 'what changes'], [
    ['`BOOST_DISABLE_THREADS`', 'no `root_ptr` lock; plain reference counts; Boost pools use `null_mutex`; page pools a no-op mutex. '
     '`std::allocator` (glibc) and `std::shared_ptr`\'s counts (libstdc++) do not change'],
    ['`BOOST_ROOT_PTR_STRIPED_LOCKS`', '`root_ptr`\'s global mutex replaced by 1024 spin locks indexed by root address'],
    ['`BOOST_PAGE_ALLOCATOR_THREAD_CACHE`', 'each thread keeps up to 16 KiB of free blocks of every page pool, moving half '
     'at once to or from the pool under its mutex; only the page allocators change'],
    ['`BOOST_ZEROIZATION`', 'every node is cleared (`memset`) before its memory is released'],
])
w('`std::unique_ptr`, `std::shared_ptr` and the allocator alone use no node and no `root_ptr` lock, so striped '
  'locks and zeroization do not change their code: they are measured in the three builds where it can differ '
  '(threads, threads + caches, no threads), and the tables show those values in *italics* for the equivalent '
  'builds.\n')
w('### Allocators, pointers, scenarios\n')
md_table(['allocator', ''], [[a, '`%s`%s' % (ALLOC_LABEL[a].replace(' (default)', ''),
                                              ', the default' if a == 'page_size' else '')] for a in ALLOC])
md_table(['pointer', 'construction'], [
    ['`boost::root_ptr`', '`root_ptr<T>(x, new node<T, A<T>>(...))`'],
    ['`boost::shared_node_ptr`', '`shared_node_ptr<T>(x, new node<T, A<T>>(...))` - what `FCXXSS_SHARED_PTR` emits: a `std::shared_ptr` owning the same node'],
    ['`std::unique_ptr`', '`allocate_unique<T>(A<T>(), ...)`, a helper whose deleter frees through the same allocator'],
    ['`std::shared_ptr`', '`std::allocate_shared<T>(A<T>(), ...)`'],
    ['allocator alone', 'one node-sized block (72 B) from the allocator, no pointer'],
])
md_table(['scenario', 'one operation'], [[s, SCEN_TEXT[s]] for s in ['raw_churn', 'raw_batch'] + SCEN])

w('## Time per operation\n')
w('ns per operation, median (lower is better). Rows: builds; columns: allocators. *Italics*: the value of the '
  'equivalent build (see Builds).\n')
for s in SCEN:
    w('### %s\n' % s)
    w(SCEN_TEXT[s] + '.\n')
    for p in PTRS:
        if s == 'cycles' and p != 'root':
            continue
        w('`%s`:\n' % PTR_LABEL[p])
        rows = []
        for b in BUILDS:
            if not applies(b, p, s):
                continue
            rows.append([BUILD_LABEL[b]] + [md_cell(*val(b, p, a, s), fmt_ns) for a in ALLOC])
        md_table(['build'] + ALLOC, rows)
w('### allocator alone\n')
for s in ('raw_churn', 'raw_batch'):
    w('%s - %s:\n' % (s, SCEN_TEXT[s]))
    md_table(['build'] + ALLOC, [[BUILD_LABEL[b]] + [md_cell(*val(b, 'raw', a, s), fmt_ns) for a in ALLOC] for b in BUILDS])

w('## Resident memory\n')
w('Resident memory grown while the objects are live, MB, median (lower is better).\n')
for s in MEM_SCEN:
    w('### %s\n' % s)
    w(SCEN_TEXT[s] + '.\n')
    for p in PTRS:
        w('`%s`:\n' % PTR_LABEL[p])
        md_table(['build'] + ALLOC, [[BUILD_LABEL[b]] + [md_cell(*val(b, p, a, s, M), fmt_mb) for a in ALLOC] for b in BUILDS])

w('## Effect of each setting\n')
w('Time without the setting / time with it, for each pair of builds that differ by that setting alone '
  '(higher is better: above 1.00x the setting is faster). *same*: the pointer\'s code does not change with it.\n')
for name, short, pairs, ptrs in EFFECTS:
    w('### %s\n' % (name[0].upper() + name[1:]))
    for off, on, label in pairs:
        w('%s: %s -> %s\n' % (label, BUILD_LABEL[off], BUILD_LABEL[on]))
        rows = []
        for p, s in effect_rows(ptrs):
            rows.append(['`%s` %s' % (PTR_LABEL[p], s)] +
                        [md_cell(*effect_cell((off, on), p, s, a)[:2], fmt_x, effect_cell((off, on), p, s, a)[2]) for a in ALLOC])
        md_table(['pointer, scenario'] + ALLOC, rows)

w('## Fastest allocator\n')
w('For every build, pointer and scenario: the fastest allocator and its time / the time of '
  '`page_allocator_by_size` (the default) in the same cell (lower is better; 1.00 = the default is the fastest).\n')
rows = []
for p, s in best_rows():
    cells = []
    for b in BUILDS:
        r = best_cell(b, p, s)
        if r is None:
            cells.append('n/a')
        else:
            a, v, rel, carried = r
            txt = a if a == 'page_size' else '%s %.2f' % (a, rel)
            cells.append('*%s*' % txt if carried else txt)
    rows.append(['`%s` %s' % (PTR_LABEL[p], s)] + cells)
md_table(['pointer, scenario'] + [BUILD_LABEL[b] for b in BUILDS], rows)

w('## The other pointers against `root_ptr`\n')
w('Time of the pointer / time of `boost::root_ptr` in the same build, allocator and scenario (lower is better: '
  'below 1.00x the pointer is faster than `root_ptr`).\n')
for a in ('page_size', 'std'):
    w('With `%s`:\n' % ALLOC_LABEL[a].replace(' (default)', ''))
    rows = []
    for s in VS_ROOT_SCEN:
        for p in ('snode', 'unique', 'shared'):
            cells = []
            for b in BUILDS:
                if not applies(b, p, s):
                    cells.append('n/a')
                    continue
                v, carried = val(b, p, a, s)
                rv = val(b, 'root', a, s)[0]
                cells.append(md_cell(v / rv if v and rv else None, carried, fmt_x))
            rows.append(['`%s` %s' % (PTR_LABEL[p], s)] + cells)
    md_table(['pointer, scenario'] + [BUILD_LABEL[b] for b in BUILDS], rows)

w('## Bulk release scaling\n')
w('ns per object, one run per size, threads (default) build (lower is better).\n')
for p in PTRS:
    w('`%s`:\n' % PTR_LABEL[p])
    ns = sorted({n for (pp, a), d in scale.items() if pp == p for n in d})
    md_table(['objects'] + ALLOC, [['{:,}'.format(n)] + [fmt_ns(scale[(p, a)][n]) if n in scale[(p, a)] else 'n/a'
                                                        for a in ALLOC] for n in ns])

with open(args.md, 'w') as fh:
    fh.write('\n'.join(out))
print(args.md)
print(args.pdf)
