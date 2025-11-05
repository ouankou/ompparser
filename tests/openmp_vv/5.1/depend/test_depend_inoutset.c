#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp single
#pragma omp task depend(out: arr[i])
#pragma omp task depend(inoutset: arr[i])
#pragma omp task depend(inoutset: arr[i])
#pragma omp target map (from: _ompvv_isOffloadingOn)
