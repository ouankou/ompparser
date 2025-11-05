#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
#pragma omp target enter data if(size > 512) map(to: size) map (to: a[0:size], b[0:size])
#pragma omp target map(tofrom: a[0:size], b[0:size], c[0:size])
#pragma omp target exit data if(size > 512) map(delete: a[0:size], b[0:size])
