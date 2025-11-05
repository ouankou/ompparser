#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(from: target_device_num) map(to: b, c) map(tofrom: a)
#pragma omp target map(from: target_device_num) device(i)
#pragma omp target map (from: _ompvv_isOffloadingOn)
