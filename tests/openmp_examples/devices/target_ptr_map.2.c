#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target map(p[:100])
