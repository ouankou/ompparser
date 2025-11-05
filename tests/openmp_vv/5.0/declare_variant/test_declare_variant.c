#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare variant(p_fn) match(construct = {parallel})
#pragma omp declare variant(t_fn) match(construct = {target})
#pragma omp for
#pragma omp declare target
#pragma omp distribute
#pragma omp end declare target
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp parallel
#pragma omp target teams map(tofrom: c[0:1024])
