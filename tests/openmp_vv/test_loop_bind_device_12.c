#pragma omp target parallel shared(result) num_threads(8) map(tofrom: result, y, z, num_threads)
