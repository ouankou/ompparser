#pragma omp parallel for allocate(x_alloc: x) private(x) shared(result) num_threads(8)
