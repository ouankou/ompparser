#pragma omp parallel shared(x) num_threads(8)
#pragma omp single
#pragma omp task shared(x, errors) depend(out: x)
#pragma omp task shared(x, errors) transparent(omp_import)
#pragma omp task shared(x, errors) depend(in: x)
