#pragma omp task depend(in: h_array) shared(sum, h_array)
