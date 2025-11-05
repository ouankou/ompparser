#pragma omp target map(tofrom: isHost) map(alloc: h_array[0:1000]) depend(inout: h_array) depend(in: val)
