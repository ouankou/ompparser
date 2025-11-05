#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel reduction(task, +: sum) num_threads(8) shared(y, z, num_threads) defaultmap(tofrom)
#pragma omp master
#pragma omp taskloop in_reduction(+: sum)
#pragma omp target map (from: _ompvv_isOffloadingOn)
