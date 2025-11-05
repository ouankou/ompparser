#pragma omp target teams distribute parallel for if(parallel: attempt >= 70) map(tofrom: a, warning) num_threads(8)
