#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp task shared(a) private(i)
#pragma omp target map(from: a)
#pragma omp parallel for
#pragma omp task shared(b) private(i)
#pragma omp target map(from: b)
#pragma omp parallel for
#pragma omp taskwait
#pragma omp task shared(c) private(i)
#pragma omp target map(from: c) map(to:a,b)
#pragma omp parallel for
#pragma omp taskwait
#pragma omp target map (from: _ompvv_isOffloadingOn)
