#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp single
#pragma omp task depend(out: c)
#pragma omp task depend(out: a)
#pragma omp task depend(out: b)
#pragma omp task depend(in: a) depend(mutexinoutset: c)
#pragma omp task depend(in: b) depend(mutexinoutset: c)
#pragma omp task depend(in: c)
