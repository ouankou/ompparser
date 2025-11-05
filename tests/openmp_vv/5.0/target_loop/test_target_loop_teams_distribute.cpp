#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams distribute parallel for map(tofrom: x [0:n]) map(to: y [0:n])
#pragma omp target teams loop map(tofrom : x [0:n]) map(to : y [0:n])
#pragma omp target map (from: _ompvv_isOffloadingOn)
