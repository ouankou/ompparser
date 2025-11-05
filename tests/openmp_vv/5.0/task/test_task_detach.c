#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel
#pragma omp single
#pragma omp task depend(out: y) detach(flag_event)
#pragma omp task
#pragma omp flush
#pragma omp task depend(inout: y)
#pragma omp flush
