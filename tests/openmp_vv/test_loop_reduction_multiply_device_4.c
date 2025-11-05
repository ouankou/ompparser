#pragma omp target parallel num_threads(8) map(tofrom: result, a, num_threads)
