#pragma omp task depend(in: B) shared(B) affinity(A[0:1024])
