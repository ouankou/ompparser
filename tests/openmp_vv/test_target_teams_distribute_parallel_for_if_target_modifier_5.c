#pragma omp target teams distribute parallel for if(target: attempt >= 70) map(tofrom: a) num_threads(8)
