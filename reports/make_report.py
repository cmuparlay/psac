#!/usr/bin/env python3

# -----------------------------------------------------------------------------
#                     Generate benchmark plots and tables
# -----------------------------------------------------------------------------
# Generates:
#   - Throughput plots for the static computation and dynamic updates
#   - Speedup and work savings table for the static computation and updates
#
# Usage: make_report.py benchmark_files... --output=output-directory --test=test-name
#
# Optionally, add --show-plots to show the plots before saving them, or
# add --aggregates if the benchmark data files contain aggregates.
# -----------------------------------------------------------------------------

import argparse
import json
import math
import matplotlib
import numpy as np
import os

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('benchmarks', type=str, nargs='+', help='The benchmark output files')

  required = parser.add_argument_group('required arguments')
  required.add_argument('--output', type=str, help='Directory to save output', required=True)
  required.add_argument('--test', type=str, help='The name of the test set', required=True)

  optional = parser.add_argument_group('optional arguments')
  optional.add_argument('--aggregates', action='store_true', dest='aggregates', default=False, help='Set if the benchmarks are aggregates (mean, med, std dev) from running repetitive benchmarks')
  optional.add_argument('--show-plots', action='store_true', dest='show_plots', default=False, help='Display the plots before directly saving them')

  args = parser.parse_args()
  
  # Use headless backend for matplotlib
  if not args.show_plots:
    matplotlib.use("Agg")
  import matplotlib.pyplot as plt
  plt.style.use('ggplot')
  
  seq_baseline = {}
  par_baseline = {}
  psac_compute = {}
  psac_update = {}
  
  # ===========================================================================
  #                              Parse the data
  # ===========================================================================

  print('Parsing data from: {}'.format(','.join(args.benchmarks)))

  for benchmark in args.benchmarks:
    benchmarks = json.load(open(benchmark, 'r'))
    
    for benchmark in benchmarks['benchmarks']:
      # Only parse mean measurements if aggregates are reported
      if args.aggregates and not benchmark['name'].endswith('mean'):
        continue;

      if 'Fixture' in benchmark['name']:
        benchmark['name'] = benchmark['name'][benchmark['name'].find('/')+1:]
        
      # ========================= Sequential basline ============================  
      if benchmark['name'].startswith(args.test + '_compute_seq'):
        name, n, _ = benchmark['name'].split('/')
        n = int(n)
        time = float(benchmark['real_time'])
        if not n in seq_baseline: seq_baseline[n] = []
        seq_baseline[n].append(time)
        
      # ========================== Parallel baseline =============================
      if benchmark['name'].startswith(args.test + '_compute_par'):
        name, p, n, _ = benchmark['name'].split('/')
        n, p = map(int, (n, p))
        time = float(benchmark['real_time'])
        if p not in par_baseline: par_baseline[p] = {}
        if n not in par_baseline[p]: par_baseline[p][n] = []
        par_baseline[p][n].append(time)
      
      # ================ Parallel self-adjusting computation =====================
      if benchmark['name'].startswith(args.test + '_compute_psac'):
        name, p, n, _ = benchmark['name'].split('/')
        n, p = map(int, (n, p))
        time = float(benchmark['real_time'])
        if p not in psac_compute: psac_compute[p] = {}
        if n not in psac_compute[p]: psac_compute[p][n] = []
        psac_compute[p][n].append(time)
        
      # ================ Parallel self-adjusting dynamic update ================== 
      if benchmark['name'].startswith(args.test + '_update_psac'):
        name, p, n, k, _ = benchmark['name'].split('/')
        n, p, k = map(int, (n, p, k))
        time = float(benchmark['real_time'])
        if p not in psac_update: psac_update[p] = {}
        if n not in psac_update[p]: psac_update[p][n] = {}
        if k not in psac_update[p][n]: psac_update[p][n][k] = []
        psac_update[p][n][k].append(time)

  # Test parameters considered
  N = list(sorted(seq_baseline.keys()))               # Input sizes considered
  P = list(sorted(par_baseline.keys()))               # Thread counts considered
  K = list(sorted(psac_update[P[0]][N[0]].keys()))    # Dynamic update sizes

  # Aggregate all of the runs into their averages
  for n in N:
    seq_baseline[n] = np.average(seq_baseline[n])
    for p in P:
      par_baseline[p][n] = np.average(par_baseline[p][n])
      psac_compute[p][n] = np.average(psac_compute[p][n])
      for k in K:
        psac_update[p][n][k] = np.average(psac_update[p][n][k])

  # Count number of cores and threads (inc. hyperthreads)
  P = P[:-1]                                          # Largest p is hyperthreading
  num_cores = max(P)
  num_threads = 2 * num_cores

  # ===========================================================================
  #                              Create output
  # ===========================================================================  
    
  # Create output file
  if not os.path.exists(args.output):
    os.makedirs(args.output)
  output = open(os.path.join(args.output, 'results.html'), 'w')
  output.write('<html><body>')
  output.write('<h1>{}</h1>'.format(args.test.replace('_', ' ')))
    
  # Save the current figure and add it to the document
  def make_figure(filename, caption):
    f = plt.gcf()
    if args.show_plots:
      f.canvas.set_window_title(caption)
      plt.show()
  
    filepath = os.path.join(args.output, filename)
    f.savefig(filepath + '.png')
    f.savefig(filepath + '.eps')
    
    output.write('<figure style="display: inline-block; "><img src="{}" style="max-height: 300px; width: auto;" /><figcaption style="text-align: center; font-weight: bold; width: 400px; caption-side: bottom;">{}</figcaption></figure>'.format(filename + '.png', caption))  
    
  # Display an amount of milliseconds in an appropriately readable way
  def tosecs(t):
    if t < 1:
      return '{:d}us'.format(int(t * 1000.0))
    elif t < 10:
      return '{:.2f}ms'.format(t)
    elif t < 1000:
      return '{:d}ms'.format(int(t))
    else:
      return '{:.2f}s'.format(t / 1000.0)

  # Write a number in a readable scaled way (e.g. 10000 -> 10k)   
  def tonum(x):
    if x < 100:
      return '{:.2f}'.format(x)
    elif x < 1000:
      return '{:.1f}'.format(x)
    elif x < 100000:
      return '{:.2f}k'.format(x / 1000)
    elif x < 1000000:
      return '{:.1f}k'.format(x / 1000)
    else:
      return '{:.2f}M'.format(x / 1000000)
 
  # ===================== Computation absolute throughput ======================
  
  output.write('<h2>{}</h2>'.format('Initial computation throughput'))  
  
  n = N[0]
  plt.clf()
  plt.xlabel('$p$ (Processors)')
  plt.ylabel('Throughput')
  plt.yscale('log')
  
  # Sequential baseline
  seq = [1000.0 * n / seq_baseline[n] for p in P]
  plt.plot(P, seq, color='green', label='Seq')
  
  # Parallel baseline
  par = [1000.0 * n / par_baseline[p][n] for p in P]
  plt.plot(P, par, color='red', label='Par')
  par_ht = 1000.0 * n / par_baseline[num_threads][n]
  plt.plot([num_cores], [par_ht], marker='+', markersize=10, color='red')
  
  # Parallel self-adjusting
  sa_comp = [1000.0 * n / psac_compute[p][n] for p in P]
  plt.plot(P, sa_comp, color='blue', label='PSAC')
  sa_ht = 1000.0 * n / psac_compute[num_threads][n]
  plt.plot([num_cores], [sa_ht], marker='+', markersize=10, color='blue')

  plt.legend(loc='best')
  
  # Make figure
  filename = '{}-{}-abs-compute-throughput'.format(args.test, n)
  caption = 'Absolute throughput in elements per second versus the number of threads (n = {})'.format(n)
  make_figure(filename, caption)

    
  # ======== Computation relative throughput (compared to sequential) ============= 
  
  n = N[0]
  plt.clf()
  plt.xlabel('$p$ (Processors)')
  plt.ylabel('Speedup')
  plt.yscale('log')  

  # Sequential baseline
  seq = 1000.0 * n / seq_baseline[n]
  
  # Parallel baseline
  par = [seq_baseline[n] / par_baseline[p][n] for p in P]
  plt.plot(P, par, color='red', label='Par')
  par_ht = seq_baseline[n] / par_baseline[num_threads][n]
  plt.plot([num_cores], [par_ht], marker='+', markersize=10, color='red')
  
  # Parallel self-adjusting
  sa_comp = [seq_baseline[n] / psac_compute[p][n] for p in P]
  plt.plot(P, sa_comp, color='blue', label='PSAC')
  sa_ht = seq_baseline[n] / psac_compute[num_threads][n]
  plt.plot([num_cores], [sa_ht], marker='+', markersize=10, color='blue')
  
  plt.legend(loc='best')
  
  # Make figure
  filename = '{}-{}-rel-compute-throughput'.format(args.test, n)
  caption = 'Relative throughput (compared to the sequential baseline) in elements per second versus the number of threads (n = {})'.format(n)
  make_figure(filename, caption)
    
  # ===================== Dynamic update absolute throughput ======================
  
  output.write('<h2>{}</h2>'.format('Dynamic update throughput'))  
  
  n = N[0]
  plt.clf()
  plt.xscale('log')
  plt.yscale('log')
  plt.xlabel('$k$ (Update size)')
  plt.ylabel('Throughput')
  
  #K = K[:-1]
  
  # Sequential baseline
  seq = [1000.0 * k / seq_baseline[n] for k in K]
  plt.plot(K, seq, color='green', marker='^', label='Seq')
  
  # Parallel baseline
  #par_all_cores = [1000.0 * k / par_baseline[num_cores][n] for k in K]
  #plt.plot(K, par_all_cores, color='red', label='Par ({})'.format(num_cores))
  
  # Parallel baseline (hyperthreaded)
  par_all_threads = [1000.0 * k / par_baseline[num_threads][n] for k in K]
  plt.plot(K, par_all_threads, color='magenta', marker='s', label='Par ({}ht)'.format(num_cores))
  
  # Self-adjusting (1 thread)
  sa_update1 = [1000.0 * k / psac_update[1][n][k] for k in K]
  plt.plot(K, sa_update1, color='cyan', marker='d', label='PSAC (1)')
  
  # Self-adjusting (all cores)
  #sa_update_all_cores = [1000.0 * k / psac_update[num_cores][n][k] for k in K]
  #plt.plot(K, sa_update_all_cores, color='blue', label='PSAC ({})'.format(num_cores))
  
  # Self-adjusting (hyperthreaded)
  sa_update_ht = [1000.0 * k / psac_update[num_threads][n][k] for k in K]
  plt.plot(K, sa_update_ht, color='black', marker='o', label='PSAC ({}ht)'.format(num_cores))
  
  plt.legend(loc='best')
  
  # Make figure
  filename = '{}-{}-abs-update-throughput'.format(args.test, n)
  caption = 'Absolute update throughput in elements per second versus the batch size of the update (n = {})'.format(n)
  make_figure(filename, caption)
  
  # ===================== Dynamic update relative throughput ======================
  
  n = N[0]
  plt.clf()
  plt.xscale('log')
  plt.yscale('log')
  plt.xlabel('$k$ (Update size)')
  plt.ylabel('Speedup')

  # Sequential baseline
  seq = {k: 1000.0 * k / seq_baseline[n] for k in K}
  
  # Parallel baseline
  #par_all_cores = [seq_baseline[n] / par_baseline[num_cores][n] for k in K]
  #plt.plot(K, par_all_cores, color='red', label='Par ({})'.format(num_cores))
  
  # Parallel baseline (hyperthreaded)
  par_all_threads = [seq_baseline[n] / par_baseline[num_threads][n] for k in K]
  plt.plot(K, par_all_threads, color='magenta', marker='s', label='Par ({}ht)'.format(num_cores))
  
  # Self-adjusting (1 thread)
  sa_update1 = [seq_baseline[n] / psac_update[1][n][k] for k in K]
  plt.plot(K, sa_update1, color='cyan', marker='d', label='PSAC (1)')
  
  # Self-adjusting (all cores)
  #sa_update_all_cores = [seq_baseline[n] / psac_update[num_cores][n][k] for k in K]
  #plt.plot(K, sa_update_all_cores, color='blue', label='PSAC ({})'.format(num_cores))
  
  # Self-adjusting (hyperthreaded)
  sa_update_ht = [seq_baseline[n] / psac_update[num_threads][n][k] for k in K]
  plt.plot(K, sa_update_ht, color='black', marker='o', label='PSAC ({}ht)'.format(num_cores))
  
  plt.legend(loc='best')
  
  # Make figure
  filename = '{}-{}-rel-update-throughput'.format(args.test, n)
  caption = 'Relative update throughput (compared to the sequential baseline) in elements per second versus the batch size of the update (n = {})'.format(n)
  make_figure(filename, caption)
  
  # ===================== Dynamic update absolute scaling ======================
  
  n = N[0]
  plt.clf()
  plt.yscale('log')
  plt.xlabel('$p$ (Processors)')
  plt.ylabel('Throughput')
  
  markers = ['v', '^', '<', '>', 's', 'p', 'h', 'd', 'o']
  marker_iter = iter(markers)
  
  for k in K:
    throughput = [1000.0 * k / psac_update[p][n][k] for p in P]
    p = plt.plot(P, throughput, marker = next(marker_iter), label='$k = 10^{}$'.format(int(math.log10(k))))
    throughput_ht = 1000.0 * k / psac_update[num_threads][n][k]
    plt.plot([num_cores], [throughput_ht], marker='+', markersize=10, color=p[0].get_color())
    
  if len(plt.gca().lines) > 2:
    plt.legend(bbox_to_anchor=(0.6, 0.0), loc="lower left")
    
  filename = '{}-{}-abs-scaling'.format(args.test, n)
  caption = 'Absolute update throughput scaling in elements per second versus the number of threads (n = {})'.format(n)
  make_figure(filename, caption)
  
# ===================== Dynamic update relative scaling ======================
  
  n = N[0]
  plt.clf()
  plt.yscale('log')
  plt.xlabel('$p$ (Processors)')
  plt.ylabel('Speedup')
  
  markers = ['v', '^', '<', '>', 's', 'p', 'h', 'd', 'o']
  marker_iter = iter(markers)
  
  for k in K:
    speedup = [seq_baseline[n] / psac_update[p][n][k] for p in P]
    p = plt.plot(P, speedup, marker = next(marker_iter), label='$k = 10^{}$'.format(int(math.log10(k))))
    speedup_ht = seq_baseline[n] / psac_update[num_threads][n][k]
    plt.plot([num_cores], [speedup_ht], marker='+', markersize=10, color=p[0].get_color())
    
  if len(plt.gca().lines) > 2:
    plt.legend(bbox_to_anchor=(0.60, 0.0), loc="lower left")
    
  filename = '{}-{}-rel-scaling'.format(args.test, n)
  caption = 'Relative update throughput (compared to the sequential baseline) scaling in elements per second versus the number of threads (n = {})'.format(n)
  make_figure(filename, caption)
  
  # ========================== Tables ==========================
  
  textable = open(os.path.join(args.output, 'table.tex'), 'w')
  n = N[0]
  
  output.write('<h2>Speedup and work savings</h2>')
  output.write('<style>table { border-collapse: collapse; } table, th, td { border: 1px solid #000; padding: 5px; }</style>')
  output.write('<table><tr><th rowspan="2">n</th><th rowspan="2">k</th><th rowspan="2">Seq</th><th colspan="4">Parallel Static</th><th colspan="4">PSAC Compute</th><th colspan="6">PSAC Dynamic Update</th></tr><tr><th>1</th><th>{0}</th><th>{0}ht</th><th>SU</th><th>1</th><th>{0}</th><th>{0}ht</th><th>SU</th><th>1</th><th>{0}</th><th>{0}ht</th><th>SU</th><th>WS</th><th>T</th></tr>'.format(num_cores))
  
  textable.write('\\begin{tabular}{| c | c | c | c | c | c | c | c | c | c | c | c | c | c | c | c | c |}')
  textable.write('\\hline \\multirow{2}{*}{$n$} & \\multirow{2}{*}{$k$} & \multirow{2}{*}{Seq} & \\multicolumn{4}{|c|}{Parallel Static} & \\multicolumn{4}{|c|}{PSAC Compute} & \\multicolumn{6}{|c|}{PSAC Update} \\\\ \\cline{4-17} \n')
  textable.write(' & & & $1$ & ${0}$ & ${0}$ht & SU & $1$ & ${0}$ & ${0}$ht & SU & $1$ & ${0}$ & ${0}$ht & SU & WS & T \\\\ \n'.format(num_cores))

  # Write table rows
  for k in K:
    output.write('<tr><td>1e{}</td><td>1e{}</td>'.format(int(math.log10(n)), int(math.log10(k))))
    textable.write('\\hline \\ $10^{}$ & $10^{}$ & '.format(int(math.log10(n)), int(math.log10(k))))
  
    # Sequential static computation
    if k == 1:
      seq_b = tosecs(seq_baseline[n])
      output.write('<td>{}</td>'.format(seq_b))
      textable.write('{} & '.format(seq_b))
    else:
      output.write('<td>-</td>')
      textable.write('- & ')

    # Parallel static computation
    if k == 1:
      sb, pb, pbht = map(tosecs, (par_baseline[1][n], par_baseline[num_cores][n], par_baseline[num_threads][n]))
      static_best = min(par_baseline[num_cores][n], par_baseline[num_threads][n])
      static_speedup = par_baseline[1][n] / par_baseline[num_threads][n]
      output.write('<td>{}</td><td>{}</td><td>{}</td><td>{:.2f}</td>'.format(sb, pb, pbht, static_speedup))
      textable.write('{} & {} & {} & {:.2f} &'.format(sb, pb, pbht, static_speedup))
    else:
      output.write('<td>{}</td><td>{}</td><td>{}</td><td>{}</td>'.format('-', '-', '-', '-'))
      textable.write('- & - & - & - &')
      
    # PSAC computation
    if k == 1:
      sb, pb, pbht = map(tosecs, (psac_compute[1][n], psac_compute[num_cores][n],psac_compute[num_threads][n]))
      psac_best = min(psac_compute[num_threads][n], psac_compute[num_cores][n])
      psac_speedup = psac_compute[1][n] / psac_best
      output.write('<td>{}</td><td>{}</td><td>{}</td><td>{:.2f}</td>'.format(sb, pb, pbht, psac_speedup))
      textable.write('{} & {} & {} & {:.2f} &'.format(sb, pb, pbht, psac_speedup))
    else:
      output.write('<td>{}</td><td>{}</td><td>{}</td><td>{}</td>'.format('-', '-', '-', '-'))
      textable.write('- & - & - & - &')
    
    # PSAC dynamic update
    seq, par, parht = map(tosecs, (psac_update[1][n][k], psac_update[num_cores][n][k], psac_update[num_threads][n][k]))
    psac_best = min(psac_update[num_threads][n][k], psac_update[num_cores][n][k]) # Hyperthreading might be bad
    update_speedup = psac_update[1][n][k] / psac_best
    update_ws = tonum(seq_baseline[n] / psac_update[1][n][k])
    total_speedup = tonum(seq_baseline[n] / psac_best)
    output.write('<td>{}</td><td>{}</td><td>{}</td><td>{:.2f}</td><td>{}</td><td>{}</td></tr>'.format(seq, par, parht, update_speedup, update_ws, total_speedup))
    textable.write('{} & {} & {} & {:.2f} & {} & {} \\\\ \n'.format(seq, par, parht, update_speedup, update_ws, total_speedup))
  
  output.write('</table>')
  textable.write('\\hline \\end{tabular}')
    
  # ===================== Finalize output ======================  
    
  output.write('</body></html>')
  output.close()
  textable.close()
    
