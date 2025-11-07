#pragma omp task shared(i)
#pragma omp task shared(j)
#pragma omp taskwait
