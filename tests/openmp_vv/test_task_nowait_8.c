#pragma omp task depend(inout : test_arr) shared(test_arr)
