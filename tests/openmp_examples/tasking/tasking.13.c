#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp task final( pos > 3 /* arbitrary limit on recursion depth */ ) mergeable
#pragma omp task final( pos > 3 /* arbitrary limit on recursion depth */ ) mergeable
#pragma omp taskwait
