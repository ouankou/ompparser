#pragma omp parallel reduction(task, +: sum) num_threads(8) shared(y, z, num_threads)
#pragma omp master
#pragma omp task in_reduction(+: sum)
