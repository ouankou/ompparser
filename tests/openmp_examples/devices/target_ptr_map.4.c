#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp metadirective when(user={condition(accessible)}: target firstprivate(ptr) ) otherwise( target map(ptr[:n]) )
