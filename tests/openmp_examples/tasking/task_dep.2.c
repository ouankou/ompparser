#pragma omp parallel
#pragma omp single
#pragma omp task shared(x) depend(in: x)
#pragma omp task shared(x) depend(out: x)
