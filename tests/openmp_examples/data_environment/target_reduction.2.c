#pragma omp target data map(sum1,sum2)
#pragma omp target teams distribute reduction(+:sum1)
#pragma omp target teams distribute map(sum1) reduction(+:sum2)
