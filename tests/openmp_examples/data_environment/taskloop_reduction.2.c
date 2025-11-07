#pragma omp taskgroup task_reduction(+: res)
#pragma omp task in_reduction(+: res)
#pragma omp taskloop in_reduction(+: res) nogroup
#pragma omp parallel
#pragma omp single
