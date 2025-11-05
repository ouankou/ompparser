#pragma omp target teams loop map(to: a[0:1024]) map(tofrom: total) reduction(+:total)
