#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target local(local_var)
#pragma omp target
#pragma omp target map(tofrom: device_value)
#pragma omp target map (from: _ompvv_isOffloadingOn)
