#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to:temp[:1])
#pragma omp target
#pragma omp target enter data map(to:cur[:1])
#pragma omp target
#pragma omp target exit data map(from:temp[0:1])
#pragma omp target exit data map(from: temp[0:1])
#pragma omp target map (from: _ompvv_isOffloadingOn)
