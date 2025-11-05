#pragma omp task depend(out: B) shared(B) affinity(A[0:1024])
