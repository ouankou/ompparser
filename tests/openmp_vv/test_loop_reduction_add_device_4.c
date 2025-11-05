#pragma omp target parallel num_threads(8) map(tofrom: total, a, b, num_threads)
