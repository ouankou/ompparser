#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target if(n > 1000000)
