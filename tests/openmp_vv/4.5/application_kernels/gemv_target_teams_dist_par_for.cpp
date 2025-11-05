#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for map(to:A[:n*n], V[:n]) map(from:Vout[:n])
#pragma omp target map (from: _ompvv_isOffloadingOn)
