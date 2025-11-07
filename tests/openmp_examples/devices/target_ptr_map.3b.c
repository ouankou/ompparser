#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target enter data map(y[:100])
#pragma omp target map(x[:100]) map(p1[:100]) map(p2[:0])
