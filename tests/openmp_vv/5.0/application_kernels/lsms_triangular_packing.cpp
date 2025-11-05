#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for collapse(4)
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to:A[0 : M*N]) map(from:buffer[0 : M*M + MR])
