!$omp       target parallel num_threads(8                       ) map(tofrom: device_result, a, num_threads)
