#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: a, b)
#pragma omp target map(from: before_value)
#pragma omp target update to(c ? a : b)
#pragma omp target map(from: after_value)
#pragma omp target update from(c ? a : b)
#pragma omp target exit data map(delete: a, b)
#pragma omp target map (from: _ompvv_isOffloadingOn)
