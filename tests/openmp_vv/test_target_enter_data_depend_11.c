#pragma omp task depend(in: h_array_copy) shared(sum, h_array_copy)
