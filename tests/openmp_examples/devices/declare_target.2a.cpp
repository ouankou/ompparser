#pragma omp begin declare target
#pragma omp end declare target
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target map(tofrom:res1)
#pragma omp target map(tofrom:res2)
#pragma omp target map(tofrom:res3)
