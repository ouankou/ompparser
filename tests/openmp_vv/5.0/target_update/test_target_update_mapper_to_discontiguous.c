#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare mapper(newvec_t v) map(v, v.data[0:v.len])
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(tofrom: s)
#pragma omp target update to(s.data[0:s.len/2:2])
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
