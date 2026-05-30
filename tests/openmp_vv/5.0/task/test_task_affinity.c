#pragma omp task depend(out: B) shared(B) affinity(A[0:1024])
#pragma omp task depend(in: B) shared(B)
#pragma omp taskwait
