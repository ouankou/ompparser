#pragma omp target map(tofrom: a, sum) depend(out: a) nowait
#pragma omp task depend(in: a) shared(a,errors)
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
