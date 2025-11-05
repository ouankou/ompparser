!$omp          task shared (h_array, in_1_ptr, in_2_ptr) depend(inout: h_array) depend(in: in_1_ptr) depend(in: in_2_ptr)
