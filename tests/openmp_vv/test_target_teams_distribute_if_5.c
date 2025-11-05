#pragma omp target teams distribute if(attempt >= 70) map(tofrom: a)
