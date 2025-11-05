#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel num_threads(8) map(tofrom: x[0:1024], num_threads, total_wait_errors) map(to: y[0:1024], z[0:1024])
#pragma omp loop order(concurrent)
#pragma omp atomic update
#pragma omp target map (from: _ompvv_isOffloadingOn)
