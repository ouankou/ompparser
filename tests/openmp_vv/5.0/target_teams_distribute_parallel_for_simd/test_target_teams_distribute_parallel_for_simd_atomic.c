#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for simd map(tofrom: x) shared(x) num_teams(8) num_threads(8)
#pragma omp atomic update
#pragma omp target map (from: _ompvv_isOffloadingOn)
