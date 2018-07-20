#!/bin/bash
#SBATCH --partition=aaiken
#SBATCH --nodes=4
#SBATCH --cpus-per-task=20
#SBATCH --exclusive
#SBATCH --time=01:00:00
#SBATCH --mail-type=ALL

function launch {
    srun -n $1 -N $1 --cpu_bind none ../../starpu/main "${@:2}" -width 20 -core 20
}

function sweep {
    for s in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        $1 $2 -kernel busy_wait -iter $(( 1 << (24-s) )) -type $3 -steps 1000
        $1 $2 -kernel busy_wait -iter $(( 3 << (22-s) )) -type $3 -steps 1000
    done
}

for n in 4 2 1; do
    for t in trivial no_comm stencil_1d stencil_1d_periodic; do
        sweep launch $n $t > starpu_type_${t}_nodes_${n}.log
    done
done