#pragma omp target map(tofrom: isHost) map (alloc: a[0:map_size], b[0:map_size], c[0:map_size])
