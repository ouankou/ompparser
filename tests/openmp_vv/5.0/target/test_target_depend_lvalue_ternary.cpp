#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel map(to:a,b,c) map(from: value)
#pragma omp single
#pragma omp task depend(out: (a > b) ? b : c) shared(a,b)
#pragma omp task depend(in: (a > b) ? b : c) shared(a, b)
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
