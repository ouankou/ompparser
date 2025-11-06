#pragma omp declare target enter(device_compute)
#pragma omp parallel masked
#pragma omp taskgroup task_reduction(+:sum)
#pragma omp target in_reduction(+:sum) nowait
#pragma omp task in_reduction(+:sum)
