#pragma omp target parallel for num_threads(8) map(to: y, z) map(tofrom: x)
