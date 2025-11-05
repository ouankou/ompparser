#pragma omp task shared(x, errors) depend(out: x)
