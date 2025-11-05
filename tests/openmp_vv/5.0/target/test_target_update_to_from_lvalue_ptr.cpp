#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: *host_pointer)
#pragma omp target map(from: before_value)
#pragma omp target update to(*host_pointer)
#pragma omp target map(from: after_value)
#pragma omp target update from(*host_pointer)
#pragma omp target exit data map(from: *host_pointer)
#pragma omp target map (from: _ompvv_isOffloadingOn)
