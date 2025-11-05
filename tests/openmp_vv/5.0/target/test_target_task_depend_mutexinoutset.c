#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(from: d)
#pragma omp parallel
#pragma omp single
#pragma omp task depend(out: c)
#pragma omp task depend(out: a)
#pragma omp task depend(out: b)
#pragma omp task depend(in: a) depend(mutexinoutset: c)
#pragma omp task depend(in: b) depend(mutexinoutset: c)
#pragma omp task depend(in: c)
#pragma omp target map (from: _ompvv_isOffloadingOn)
