#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target enter data map(A[:N], B[:N], D[:N])
#pragma omp target map(tofrom: errors)
#pragma omp target exit data map(B[:N], A[:N],D[:N])
