#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel num_threads(2)
#pragma omp masked
#pragma omp task detach(event) if(0)
#pragma omp task
#pragma omp task
