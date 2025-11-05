!$omp   target parallel num_threads(8                       ) map(tofrom: a, b, c, d, num_threads)
