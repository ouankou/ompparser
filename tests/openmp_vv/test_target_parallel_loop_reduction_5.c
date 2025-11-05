#pragma omp target parallel loop reduction(max:result) map(to: a[0:1024], b[0:1024]) map(tofrom: result)
