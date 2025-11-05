#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: a[:100], b[:100]) map(tofrom: c)
#pragma omp target
#pragma omp target update if (change_flag) to(b[:100])
#pragma omp target
