!$omp          target enter data map(alloc: h_array) map(to: in_1_ptr) map(to: in_2_ptr) depend(out: h_array) depend(in: in_1_ptr) depend(in: in_2_ptr)
