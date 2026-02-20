#* ------------
#* CSC367 Experiment Automation Script (Extended)
#* -------------

import subprocess
import os
import re
from collections import defaultdict
import pickle
import matplotlib
matplotlib.use('Agg')
import plotter

# ----------------------------
# Configuration
# ----------------------------

threads = [1, 2, 4, 8]
chunk_sizes = [1, 2, 4, 8, 16, 32]

methods = {
    "sequential": 1,
    "sharded_rows": 2,
    "sharded_columns column major": 3,
    "sharded_columns row major": 4,
    "work queue": 5,
}

filters = {
    "1x1": 4,
    "3x3": 1,
    "5x5": 2,
    "9x9": 3,
}

default_file = '1'
results = defaultdict()

# ----------------------------
# Utilities
# ----------------------------

def execute_command(command):
    print(">>>>> Executing command {}".format(command))
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        shell=True,
        universal_newlines=True,
    )
    return_code = process.wait()
    output = process.stdout.read()

    if return_code == 1:
        print("Command failed:", command)
        print(output)
        exit()

    return output


def parse_perf(x):
    d = {}
    d['dump'] = x
    x = x.replace(',', '')

    items = {
        'seconds time elapsed': 'time',
        'instructions:u': 'instructions',
        'L1-dcache-loads': 'l1d_load',
        'L1-dcache-load-misses': 'l1d_loadmisses',
        'LLC-loads': 'll_load',
        'LLC-load-misses': 'll_loadmisses',
    }

    fp_regex = ('.*\s((\d+\.\d*)|(\d+))\s*{}\s*(#.*L1-dcache [a-z]+)?\s*'
                + re.escape('( +-') + '\s*(\d+\.\d+)' + re.escape('% )') + '.*')

    for name, key in items.items():
        vals = re.match(fp_regex.format(name), x, flags=re.DOTALL)
        if vals is not None:
            d[key] = vals[1]

    return d


if os.path.isfile('data.pickle'):
    with open('data.pickle', 'rb') as f:
        results = pickle.load(f)


def run_perf(filter, method, numthreads=1, chunk_size=1,
             input_file=default_file, repeat=5):

    key = (filter, method, numthreads, chunk_size, input_file)
    if results.get(key) is not None:
        return

    main_args = './main.out -t {} -b {} -f {} -m {} -n {} -c {}'.format(
        0, input_file, filters[filter],
        methods[method], numthreads, chunk_size)

    # Cold run
    execute_command('perf stat ' + main_args)

    counters = [
        'instructions:u',
        'L1-dcache-loads:u',
        'L1-dcache-load-misses:u',
        'LLC-loads:u',
        'LLC-load-misses:u',
    ]

    groups = [counters[i:i+4] for i in range(0, len(counters), 4)]
    partial = {}

    for group in groups:
        command = 'perf stat -r {} -e {} {}'.format(
            repeat, ",".join(group), main_args)
        ret = execute_command(command)
        parsed = parse_perf(ret)
        partial = {**parsed, **partial}

    # Time measurement
    main_args_time = './main.out -t {} -b {} -f {} -m {} -n {} -c {}'.format(
        1, input_file, filters[filter],
        methods[method], numthreads, chunk_size)

    total = 0
    for _ in range(repeat):
        ret = execute_command(main_args_time)
        total += float(ret[5:])
    partial['time'] = total / repeat

    # L1 miss rate
    if 'l1d_loadmisses' in partial and 'l1d_load' in partial:
        partial['l1_miss_rate'] = (
            float(partial['l1d_loadmisses']) /
            float(partial['l1d_load'])
        )

    results[key] = partial

    with open('data.pickle', 'wb') as f:
        pickle.dump(results, f, pickle.HIGHEST_PROTOCOL)


# ----------------------------
# Experiments
# ----------------------------

def thread_scaling(filter="3x3"):
    local = defaultdict(list)

    # First get sequential baseline once
    run_perf(filter, "sequential", 1, 1)
    seq_key = (filter, "sequential", 1, 1, default_file)
    seq_time = float(results[seq_key]['time'])

    for method in methods:
        for n in threads:

            if method == "sequential":
                # replicate baseline across thread counts
                local[method].append(seq_time)
                continue

            chunk = n
            run_perf(filter, method, n, chunk)
            key = (filter, method, n, chunk, default_file)
            local[method].append(float(results[key]['time']))

    plotter.graph(
        threads,
        [local[m] for m in methods],
        list(methods.keys()),
        ['r','b','g','c','m'],
        'thread_scaling_{}.png'.format(filter),
        'Thread Scaling (filter={})'.format(filter),
        "# Threads",
        "Time (s)"
    )


def workqueue_chunks(filter="3x3"):
    for n in threads:
        yvals = []
        for chunk in chunk_sizes:
            run_perf(filter, "work queue", n, chunk)
            key = (filter, "work queue", n, chunk, default_file)
            yvals.append(float(results[key]['time']))

        plotter.graph(
            chunk_sizes,
            [yvals],
            ["{} threads".format(n)],
            ['b'],
            'workqueue_chunks_{}_{}.png'.format(filter, n),
            'Work Queue Chunk Study ({} threads)'.format(n),
            "Chunk Size",
            "Time (s)"
        )


def filter_scaling():
    fixed_threads = 8
    local = defaultdict(list)

    for f in filters:
        for method in methods:
            chunk = fixed_threads
            run_perf(f, method, fixed_threads, chunk)
            key = (f, method, fixed_threads, chunk, default_file)
            local[method].append(float(results[key]['time']))

    plotter.graph(
        list(filters.keys()),
        [local[m] for m in methods],
        list(methods.keys()),
        ['r','b','g','c','m'],
        'filter_scaling.png',
        'Filter Size Scaling (8 threads)',
        "Filter",
        "Time (s)"
    )


# ----------------------------
# Main Driver
# ----------------------------

if __name__ == "__main__":
    for size in ["1x1", "3x3", "5x5", "9x9"]:
        thread_scaling(size)
        workqueue_chunks(size)
        filter_scaling()

