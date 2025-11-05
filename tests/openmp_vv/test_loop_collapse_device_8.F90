!$omp     target parallel num_threads(8                       ) map(tofrom: a, b, num_threads)
