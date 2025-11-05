#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp threadprivate(GlobalVar)
#pragma omp parallel
#pragma omp task if(0)
