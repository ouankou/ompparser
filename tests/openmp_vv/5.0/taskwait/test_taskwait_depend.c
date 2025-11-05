#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel for
#pragma omp task depend(inout: x) shared(x)
#pragma omp task depend(inout: y) shared(y)
#pragma omp taskwait depend(in: x)
#pragma omp taskwait depend(in: x,y)
#pragma omp atomic
