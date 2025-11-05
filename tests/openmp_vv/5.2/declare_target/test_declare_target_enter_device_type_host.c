#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target enter(a,b,c,i)
#pragma omp declare target enter(update) device_type(host)
#pragma omp target update to(a,b,c)
#pragma omp target
#pragma omp target update from( a, b, c)
