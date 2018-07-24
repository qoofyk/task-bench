# A Task Benchmark [![Build Status](https://travis-ci.org/StanfordLegion/task-bench.svg?branch=master)](https://travis-ci.org/StanfordLegion/task-bench)

## Quickstart

```
git clone https://github.com/StanfordLegion/task-bench.git
cd task-bench
./get_deps.sh
./build_all.sh
./legion/task_bench -steps 9 -type dom

./task_bench -steps 4 -type fft -width 4 -kernel compute_bound -num_src_input 1 -size 512 -max_power 200 -iter 20
./task_bench -steps 4 -type dom -width 4 -kernel memory_bound -num_src_input 2 -size 512 -jump 64 -iter 2000
```
