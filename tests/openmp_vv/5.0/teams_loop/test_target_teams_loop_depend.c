#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target teams loop map(tofrom: a[0:1024]) depend(out: a[0:1024]) nowait
#pragma omp target teams loop map(tofrom: a[0:1024], b[0:1024]) depend(in: a[0:1024]) depend(out: b[0:1024]) nowait
#pragma omp target teams loop map(tofrom: b[0:1024], c[0:1024]) depend(in: b[0:1024])
