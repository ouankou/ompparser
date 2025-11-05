!$omp     parallel do order(concurrent) num_threads(8) shared(x, y, z)
