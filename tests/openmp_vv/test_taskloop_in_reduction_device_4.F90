!$omp     target parallel reduction(task, +:test_sum) num_threads(8) shared(y, z, num_threads) defaultmap(tofrom)
