#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare mapper(myvec_t v) map(v, v.data[0:v.len])
#pragma omp target
#pragma omp target map (from: _ompvv_isOffloadingOn)
