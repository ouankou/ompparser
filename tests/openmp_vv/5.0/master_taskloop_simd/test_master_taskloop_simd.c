#pragma omp parallel num_threads(8) shared(x, y, z, num_threads)
#pragma omp master taskloop simd
