#pragma omp taskgroup task_reduction(+: result)
#pragma omp task in_reduction(+: result)
#pragma omp parallel shared(result) num_threads(8)
#pragma omp single
