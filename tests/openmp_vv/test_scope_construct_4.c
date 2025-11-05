#pragma omp target parallel shared(total) map(tofrom : total)
