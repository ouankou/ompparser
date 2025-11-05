#pragma omp target map(alloc: h_matrix[0:1000]) map(tofrom: isHost[dev:1])
