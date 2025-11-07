#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(3)
#pragma omp flush
#pragma omp atomic update
#pragma omp atomic read
#pragma omp flush
#pragma omp flush(data1)
#pragma omp atomic update
#pragma omp atomic read
#pragma omp flush(data0,data1)
