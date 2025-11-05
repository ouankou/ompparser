!$omp     target parallel num_threads(8                       ) map(tofrom: x, y, z, num_threads)
