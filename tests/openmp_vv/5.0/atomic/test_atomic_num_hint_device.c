#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: num_threads,x)
#pragma omp parallel num_threads(8) default(shared)
#pragma omp atomic hint(0X4)
#pragma omp target map(tofrom: num_threads,x)
#pragma omp parallel num_threads(8) default(shared)
#pragma omp atomic hint(0X1024)
#pragma omp target map (from: _ompvv_isOffloadingOn)
