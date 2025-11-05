#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: c ? a : b)
#pragma omp target map(from: before_value)
#pragma omp target update to(a)
#pragma omp target map(from: after_value)
#pragma omp target update from(a)
#pragma omp target exit data map(delete: c ? a : b)
#pragma omp target map (from: _ompvv_isOffloadingOn)
