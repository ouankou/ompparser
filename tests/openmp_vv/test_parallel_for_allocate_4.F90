!$omp     parallel do allocate(x_alloc: x) private(x) shared(result_arr)num_threads(8)
