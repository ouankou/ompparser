#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: a[0:1024], b[0:1024]) map(tofrom: c[0:1024])
#pragma omp target teams distribute lastprivate(privatized) map(alloc: a[0:1024], b[0:1024], c[0:1024]) defaultmap(tofrom:scalar)
#pragma omp target data map(to: a[0:1024], b[0:1024], c[0:1024]) map(tofrom: privatized_array[0:2])
#pragma omp target teams distribute lastprivate(privatized_array) map(alloc: a[0:1024], b[0:1024], c[0:1024])
