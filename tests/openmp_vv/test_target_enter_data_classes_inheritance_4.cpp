#pragma omp target enter data map(to:solutionPtr[0:1]) if(not_mapped)
