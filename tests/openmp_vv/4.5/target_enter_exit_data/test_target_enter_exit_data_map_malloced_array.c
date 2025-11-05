#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: x[:10])
#pragma omp target
#pragma omp target exit data map(from: x[:10])
#pragma omp target data map(tofrom: x[:10]) map(from: y[:10])
#pragma omp target exit data map(delete: x[:10])
#pragma omp target map(to: x[:10])
#pragma omp target map (from: _ompvv_isOffloadingOn)
