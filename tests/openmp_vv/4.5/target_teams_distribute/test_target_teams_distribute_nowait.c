#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target enter data map(to: ticket[0:1], order[0:16])
#pragma omp target teams distribute map(alloc: work_storage[i][0:1024], ticket[0:1]) nowait
#pragma omp atomic capture
#pragma omp taskwait
#pragma omp target exit data map(from:ticket[0:1], order[0:16])
