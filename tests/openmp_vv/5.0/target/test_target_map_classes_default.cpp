#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(this->h_array[0:1000]) map(size)
#pragma omp target
#pragma omp declare target
#pragma omp end declare target
#pragma omp target map(tofrom: res)
#pragma omp target map(tofrom: value)
#pragma omp target map (from: _ompvv_isOffloadingOn)
