#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare mapper(custom: T v) map(to: v, v.len, v.data[0:v.len])
#pragma omp target data map(mapper(custom), to:s)
#pragma omp target
#pragma omp target update from(s.data[:1024/2:2])
#pragma omp target map (from: _ompvv_isOffloadingOn)
