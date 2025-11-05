#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target_data map(tofrom : a)
#pragma omp target map(tofrom : a) nowait
#pragma omp target_data map(tofrom : a) nogroup
#pragma omp target map(tofrom : a) nowait
#pragma omp target_data map(tofrom : a) depend(inout : a)
#pragma omp target map(tofrom : a) nowait depend(out : a)
#pragma omp target map (from: _ompvv_isOffloadingOn)
