#pragma omp target data map(to:A[0 : M*N]) map(from:buffer[0 : M*M + MR])
