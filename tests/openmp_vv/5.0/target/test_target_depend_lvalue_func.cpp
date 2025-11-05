#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target
#pragma omp end declare target
#pragma omp target parallel map(tofrom: value)
#pragma omp single
#pragma omp task depend(out: ref())
#pragma omp task depend(in: ref())
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
