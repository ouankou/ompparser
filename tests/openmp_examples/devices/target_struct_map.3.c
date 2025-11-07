#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target data map(S1.p[:100])
#pragma omp target
#pragma omp target data map(S2.p[:100])
#pragma omp target map(S2.p, S2.a, S2.b)
