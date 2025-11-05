#pragma omp target parallel num_threads(2) map(tofrom: x, y, errors)
