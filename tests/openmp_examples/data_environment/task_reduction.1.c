#pragma omp taskgroup task_reduction(+: res)
#pragma omp task in_reduction(+: res)
#pragma omp parallel
#pragma omp single
