#pragma omp target teams distribute parallel for if(attempt >= 70) map(tofrom: a, warning) num_threads(8)
