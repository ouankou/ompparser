#pragma omp target data if(map_size > 512) map(to: map_size) map(tofrom: c[0:map_size]) map(to: a[0:map_size], b[0:map_size])
