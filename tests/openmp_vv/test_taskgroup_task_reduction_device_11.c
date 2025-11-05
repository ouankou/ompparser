#pragma omp target parallel shared(result) num_threads(8) defaultmap(tofrom) map(root[:1])
