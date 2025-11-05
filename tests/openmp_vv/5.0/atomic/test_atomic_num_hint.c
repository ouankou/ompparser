#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(2) default(shared)
#pragma omp atomic hint(0X4)
#pragma omp parallel num_threads(8) default(shared)
#pragma omp atomic hint(0X1024)
