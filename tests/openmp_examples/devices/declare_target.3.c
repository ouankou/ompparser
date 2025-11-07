#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target update to(v1, v2)
#pragma omp target
#pragma omp parallel for
#pragma omp target update from(p)
