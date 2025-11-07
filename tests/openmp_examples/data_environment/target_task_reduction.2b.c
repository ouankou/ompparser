#pragma omp declare target enter(device_compute)
#pragma omp parallel masked reduction(task, +:sum)
#pragma omp target in_reduction(+:sum) nowait
