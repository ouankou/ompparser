#pragma omp target exit data map(delete: solutionPtr[0:1]) if(not_mapped)
