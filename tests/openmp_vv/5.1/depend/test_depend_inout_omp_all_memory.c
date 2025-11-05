#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map(tofrom: errors) map(to: x, y)
#pragma omp parallel
#pragma omp single
#pragma omp task depend(out: x)
#pragma omp task depend(out: y)
#pragma omp task depend(inout: omp_all_memory)
#pragma omp task depend(out: x)
#pragma omp task depend(out: y)
