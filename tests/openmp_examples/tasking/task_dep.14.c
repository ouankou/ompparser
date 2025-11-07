#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel single
#pragma omp task depend(inout:h) transparent(omp_impex)
#pragma omp task depend(inout:M[i*20])
