#pragma omp parallel for reduction(task, +: sum) num_threads(8) shared(y, z, num_threads)
