#pragma omp target parallel num_threads(8) shared(x, y, z, num_threads) map(tofrom: x, y, z, num_threads)
