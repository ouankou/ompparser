!$omp     parallel master taskloop simd num_threads(8) shared(x, y, z, num_threads)
