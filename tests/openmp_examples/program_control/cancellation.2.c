#pragma omp task shared(found) if(level < 10)
#pragma omp atomic write
#pragma omp cancel taskgroup
#pragma omp task shared(found) if(level < 10)
#pragma omp atomic write
#pragma omp cancel taskgroup
#pragma omp taskwait
#pragma omp parallel shared(found, tree, value)
#pragma omp masked
#pragma omp taskgroup
