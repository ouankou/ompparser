#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel loop reduction(min:result) map(to: a[0:1024], b[0:1024]) map(tofrom: result)
#pragma omp target parallel loop reduction(max:result) map(to: a[0:1024], b[0:1024]) map(tofrom: result)
#pragma omp target parallel loop reduction(+:total) map(to: arr[0:1024]) map(tofrom: total)
