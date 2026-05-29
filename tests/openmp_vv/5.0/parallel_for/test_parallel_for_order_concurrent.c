#pragma omp parallel for order(concurrent) num_threads(8) shared(x, y, z)
