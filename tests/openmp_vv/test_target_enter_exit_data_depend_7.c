#pragma omp task shared (h_array, in_1, in_2) depend(inout: h_array) depend(in: in_1) depend(in: in_2)
