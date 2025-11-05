#pragma omp task shared (h_array, h_array_copy) depend(in: h_array) depend(out: h_array_copy)
