#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(2)
#pragma omp flush
#pragma omp atomic write
#pragma omp atomic read
#pragma omp flush
#pragma omp parallel num_threads(2)
#pragma omp flush seq_cst
#pragma omp atomic write
#pragma omp atomic read
#pragma omp flush seq_cst
#pragma omp target map (from: _ompvv_isOffloadingOn)
