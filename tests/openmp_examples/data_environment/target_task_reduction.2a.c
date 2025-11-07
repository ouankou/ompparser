#pragma omp declare target enter(device_compute)
#pragma omp parallel sections reduction(task, +:sum)
#pragma omp section
#pragma omp target in_reduction(+:sum)
#pragma omp section
