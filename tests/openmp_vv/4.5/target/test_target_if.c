#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target if(size > 512) map(to: size) map(tofrom: c[0:size]) map(to: a[0:size], b[0:size]) map(tofrom: isHost)
