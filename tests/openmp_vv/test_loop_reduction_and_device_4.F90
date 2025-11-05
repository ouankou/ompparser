!$omp        target parallel num_threads(8                       ) map(tofrom: test_result, a, num_threads)
