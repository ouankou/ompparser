#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: a, sum) depend(out: a) nowait
#pragma omp task depend(in: a) shared(a,errors)
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
