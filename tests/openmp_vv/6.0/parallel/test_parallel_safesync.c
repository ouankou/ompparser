#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target thread_limit(8) map(to : count1, count2) map(a) map(to : b) map(tofrom : num_threads)
#pragma omp parallel num_threads(8) safesync(1)
#pragma omp atomic capture
#pragma omp atomic read acquire
#pragma omp atomic release
#pragma omp target map (from: _ompvv_isOffloadingOn)
