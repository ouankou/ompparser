#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(to: isSharedMemory)
#pragma omp target firstprivate(ptr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
