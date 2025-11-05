#pragma omp task shared(x, y) depend(out: omp_all_memory)
