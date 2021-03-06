#!/usr/bin/env python3

# Copyright 2019 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import argparse
import collections
import csv
import numpy
import os
import re
import sys

_columns = collections.OrderedDict([
    ('elapsed', (re.compile(r'^\s*Elapsed Time ([0-9.e+-]+) seconds$', re.MULTILINE), float)),
    ('iterations', (re.compile(r'^\s*Iterations: ([0-9]+)$', re.MULTILINE), int)),
    ('steps', (re.compile(r'^\s*Time Steps: ([0-9]+)$', re.MULTILINE), int)),
    ('tasks', (re.compile(r'^\s*Total Tasks ([0-9]+)$', re.MULTILINE), int)),
    ('flops', (re.compile(r'^\s*Total FLOPs ([0-9]+)$', re.MULTILINE), int)),
    ('bytes', (re.compile(r'^\s*Total Bytes ([0-9]+)$', re.MULTILINE), int)),
    ('width', (re.compile(r'^\s*Max Width: ([0-9]+)$', re.MULTILINE), int)),
])

def same(values):
    return all(value == values[0] for value in values)

def group_by(keys, values):
    last_key = None
    last_group = None
    for key, value in zip(keys, values):
        if key != last_key:
            if last_group is not None:
                yield (last_key, last_group)
            last_group = []
        last_key = key
        last_group.append(value)

    if last_group is not None:
        yield (last_key, last_group)

def analyze(filename, ngraphs, nodes, cores, threshold, peak_flops, peak_bytes, summary=True):
    compute = collections.OrderedDict([
        ('scale_factor', lambda t: t['iterations'][0] / t['iterations']),
        ('time_per_task', lambda t: t['elapsed'] / t['tasks'] * nodes * cores * 1000),
        ('flops_per_second', lambda t: t['flops'] / t['elapsed']),
        ('bytes_per_second', lambda t: t['bytes'] / t['elapsed']),
    ])

    if peak_flops:
        compute['efficiency'] = lambda t: (t['flops'] / t['elapsed'] / nodes) / peak_flops
    elif peak_bytes:
        compute['efficiency'] = lambda t: (t['bytes'] / t['elapsed'] / nodes) / peak_bytes
    else:
        compute['efficiency'] = lambda t: t['elapsed'][0] / (t['elapsed'] * t['scale_factor'])

    # Parse input columns:
    with open(filename) as f:
        text = f.read()
    table = collections.OrderedDict(
        (k, numpy.asarray([t(m.group(1)) for m in re.finditer(p, text)]))
        for k, (p, t) in _columns.items())

    assert table['tasks'].size > 0, "logs are empty"

    for column in ('iterations', 'steps', 'width'):
        assert table[column].size % ngraphs == 0, "number of graphs is not divisible by ngraphs"
        elts = numpy.split(table[column], table[column].size / ngraphs)
        assert all(same(elt) for elt in elts), "graphs are not identical"
        table[column] = table[column][::ngraphs]

    # Check consistency of data:
    assert same([len(column) for column in table.values()]), "columns are uneven"
    assert same(table['steps']), "steps are not identical"
    assert same(table['width']), "widths are not identical"
    assert same(table['tasks']), "tasks are not identical"
    assert all(table['tasks'] == ngraphs * table['steps'] * table['width']), "mismatch in tasks vs steps * width"

    # Group by iteration count and compute statistics:
    table['iterations'], table['elapsed'], table['std'], table['reps'], table['flops'], table['bytes'], same_flops, same_bytes = list(map(
        numpy.asarray,
        zip(*[(k, numpy.mean(elapsed), numpy.std(elapsed), len(elapsed), flops[0], bytes[0], same(flops), same(bytes))
              for k, vs in group_by(table['iterations'], zip(table['elapsed'], table['flops'], table['bytes']))
              for elapsed, flops, bytes in [zip(*vs)]])))

    assert all(same_flops), "flops are not identical"
    assert all(same_bytes), "bytes are not identical"

    for column in ('steps', 'width', 'tasks'):
        table[column] = numpy.resize(table[column], table['iterations'].shape)

    # Compute derived columns:
    for k, f in compute.items():
        table[k] = f(table)

    # Post-process table for output:
    result = collections.OrderedDict()
    for k, c in table.items():
        if any(isinstance(x, float) for x in c):
            result[k] = ['%e' % x for x in c]
        else:
            result[k] = table[k]

    if not summary:
        return result

    out_filename = os.path.splitext(filename)[0] + '.csv'
    with open(out_filename, 'w') as f:
        out = csv.writer(f)
        out.writerow(result.keys())
        out.writerows(zip(*list(result.values())))

    # Compute minimum efficient task granularity:
    min_time = None
    if summary:
        assert any(table['efficiency'] >= threshold), "no data above threshold, was run properly configured?"
        assert any(table['efficiency'] < threshold), "no data below threshold, maybe run was truncated?"

        # Find smallest task granularity above efficiency threshold:
        min_i, min_efficiency = min(
            filter(lambda x: x[1] >= threshold,
                   enumerate(table['efficiency'])),
            key=lambda x: table['time_per_task'][x[0]])

        assert min_i + 1 < len(table['reps']), "no data following the point above the threshold"
        assert table['reps'][min_i + 1] >= table['reps'][min_i], "final data point has fewer reps, maybe run was truncated?"

        # Perform linear interpolation if subsequent data point is an improvment:
        min_time = table['time_per_task'][min_i]
        if len(table['time_per_task']) > min_i + 1 and table['time_per_task'][min_i + 1] < min_time:
            min_time = numpy.interp(
                threshold,
                numpy.flip(table['efficiency'][min_i:min_i+2], 0),
                numpy.flip(table['time_per_task'][min_i:min_i+2], 0))

    return min_time

def driver(inputs, summary, ngraphs, nodes, cores, threshold, peak_flops, peak_bytes):
    if peak_flops is not None and peak_bytes is not None:
        raise Exception('Can specify at most one --peak-* flag')
    min_times = []
    for filename in inputs:
        min_times.append(analyze(filename, ngraphs, nodes, cores, threshold, peak_flops, peak_bytes))
    if summary:
        with open(summary, 'w') as f:
            out = csv.writer(f)
            out.writerow(['effective_minimum_granularity'])
            out.writerows(zip(min_times))
    print(min_times)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('inputs', nargs='+')
    parser.add_argument('-g', '--ngraphs', type=int, required=True)
    parser.add_argument('-n', '--nodes', type=int, required=True)
    parser.add_argument('-c', '--cores', type=int, required=True)
    parser.add_argument('-t', '--threshold', type=float, default=0.5)
    parser.add_argument('--peak-compute-bandwidth', type=float, default=None,
                        dest='peak_flops',
                        help='peak compute bandwidth per node in DP FLOP/s')
    parser.add_argument('--peak-memory-bandwidth', type=float, default=None,
                        dest='peak_bytes',
                        help='peak memory bandwidth per node in B/s')
    parser.add_argument('-s', '--summary')
    args = parser.parse_args()
    driver(**vars(args))
