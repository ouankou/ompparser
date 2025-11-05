#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp task shared(sum) default(private)
#pragma omp target map(tofrom: sum, test_num)
#pragma omp target map (from: _ompvv_isOffloadingOn)
