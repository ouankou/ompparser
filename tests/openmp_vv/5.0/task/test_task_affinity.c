#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp task depend(out: B) shared(B) affinity(A[0:1024])
#pragma omp task depend(in: B) shared(B)
#pragma omp taskwait
