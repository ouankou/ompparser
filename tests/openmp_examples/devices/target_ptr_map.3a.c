#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target enter data map(x[:100])
#pragma omp target
