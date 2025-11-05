#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin assumes contains(target, parallel, for)
#pragma omp end assumes
#pragma omp target map(tofrom: arr)
#pragma omp parallel num_threads(8)
#pragma omp for
#pragma omp target map (from: _ompvv_isOffloadingOn)
