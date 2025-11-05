#pragma omp target update depend(inout: h_array) from(h_array[0:1000])
