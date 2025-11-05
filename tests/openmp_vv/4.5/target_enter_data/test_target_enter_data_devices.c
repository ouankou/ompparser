#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(alloc: h_matrix[dev][0:1000])
#pragma omp target map(alloc: h_matrix[dev][0:1000]) map(tofrom: isHost[dev:1])
#pragma omp target map(from: h_matrix_copy[dev][0:1000]) map(alloc: h_matrix[dev][0:1000])
#pragma omp target exit data map(delete: h_matrix[dev][0:1000]) device(dev)
#pragma omp target enter data map(alloc: h_matrix[dev][0:1000]) device(dev)
#pragma omp target map(alloc: h_matrix[dev][0:1000]) map(tofrom: isHost[dev:1]) device(dev)
#pragma omp target map(from: h_matrix_copy[dev][0:1000]) map(alloc: h_matrix[dev][0:1000]) device(dev)
#pragma omp target exit data map(delete: h_matrix[dev][0:1000]) device(dev)
#pragma omp target map (from: _ompvv_isOffloadingOn)
