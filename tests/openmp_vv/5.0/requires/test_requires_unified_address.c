#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires unified_address
#pragma omp target map(from:mem_ptr2) defaultmap(firstprivate)
#pragma omp target map(tofrom:errors) defaultmap(to)
#pragma omp target map (from: _ompvv_isOffloadingOn)
