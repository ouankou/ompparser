#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp begin declare variant match(implementation={vendor(nvidia)})
#pragma omp begin declare variant match(device={isa(sm_70)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={isa(sm_80)})
#pragma omp end declare variant
#pragma omp end declare variant
#pragma omp begin declare variant match(implementation={vendor(amd)})
#pragma omp end declare variant
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp target
#pragma omp target map(tofrom: array[0:64])
