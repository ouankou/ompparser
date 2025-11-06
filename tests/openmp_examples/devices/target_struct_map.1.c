#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target map(alloc:S.p) map(S.p[:100]) map(to:S.a, S.b)
