#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp taskwait
#pragma omp target is_device_ptr(devRect) device(t)
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
