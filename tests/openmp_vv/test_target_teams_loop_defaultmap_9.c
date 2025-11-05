#pragma omp target teams loop defaultmap(tofrom:aggregate) map(tofrom: host_data)
