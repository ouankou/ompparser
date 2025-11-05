#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp requires atomic_default_mem_order(seq_cst)
#pragma omp parallel
#pragma omp atomic write
#pragma omp atomic read
