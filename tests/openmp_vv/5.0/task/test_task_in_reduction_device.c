#pragma omp target parallel reduction(task, +: sum) num_threads(8) shared(y, z, num_threads) map(tofrom: sum, num_threads) map(to: y, z)
#pragma omp master
#pragma omp task in_reduction(+: sum)
#pragma omp target map (from: _ompvv_isOffloadingOn)
