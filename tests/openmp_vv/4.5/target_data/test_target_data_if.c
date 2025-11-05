#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data if(map_size > 512) map(to: map_size) map(tofrom: c[0:map_size]) map(to: a[0:map_size], b[0:map_size])
#pragma omp target if(map_size > 512) map(tofrom: isHost) map (alloc: a[0:map_size], b[0:map_size], c[0:map_size])
#pragma omp target data if(map_size > 512) map(to: map_size) map(tofrom: c[0:map_size]) map(to: a[0:map_size], b[0:map_size])
#pragma omp target map(tofrom: isHost) map (alloc: a[0:map_size], b[0:map_size], c[0:map_size])
#pragma omp target map (from: _ompvv_isOffloadingOn)
