#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for map(tofrom: num_threads) num_threads(tested_num_threads[nt]) thread_limit(tested_thread_limit[tl])
#pragma omp target map (from: _ompvv_isOffloadingOn)
