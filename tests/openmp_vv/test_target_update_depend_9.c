#pragma omp target map(tofrom: isHost) map(alloc: in_1[0:1000]) map(alloc: in_2[0:1000]) map(alloc: h_array[0:1000])
