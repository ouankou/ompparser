#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target data map(S1.p[:100],S1.p,S1.a,S1.b)
#pragma omp target
#pragma omp target data map(S2.p[:100])
#pragma omp target map(S2.p[:0], S2.a, S2.b)
#pragma omp target map(S3.p[:100], S3.a, S3.b)
