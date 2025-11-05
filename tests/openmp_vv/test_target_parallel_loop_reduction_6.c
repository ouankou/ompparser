#pragma omp target parallel loop reduction(+:total) map(to: arr[0:1024]) map(tofrom: total)
