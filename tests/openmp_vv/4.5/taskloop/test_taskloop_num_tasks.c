#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(100)
#pragma omp single
#pragma omp taskloop num_tasks(6)
#pragma omp atomic
