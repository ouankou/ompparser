#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target
#pragma omp end declare target
#pragma omp target map(from: before_value) map(returnRef())
#pragma omp target update to(returnRef())
#pragma omp target map(from: after_value)
#pragma omp target update from(returnRef())
#pragma omp target map (from: _ompvv_isOffloadingOn)
