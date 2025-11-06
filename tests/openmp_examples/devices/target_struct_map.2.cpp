#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target map(alloc:p) map(to:p[:100]) map(to:a,b) map(from:buffer[:100])
