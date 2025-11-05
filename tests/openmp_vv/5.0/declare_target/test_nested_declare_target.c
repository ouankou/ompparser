#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target
#pragma omp declare target link(a,b,c)
#pragma omp end declare target
#pragma omp declare target
#pragma omp declare target to(update)
#pragma omp end declare target
#pragma omp target map(tofrom: a, b, c)
