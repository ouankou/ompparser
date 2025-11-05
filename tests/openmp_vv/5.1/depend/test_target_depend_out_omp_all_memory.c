#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: errors) map(to: x, y)
#pragma omp parallel
#pragma omp single
#pragma omp task shared(x, y) depend(out: omp_all_memory)
#pragma omp task shared(x, y, errors) depend(in: x, y)
#pragma omp taskwait
