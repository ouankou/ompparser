!$omp     target parallel shared(outData) num_threads(8) private(x) map(tofrom: outData, y, z, num_threads)
