#pragma omp declare simd linear(p:1) notinbranch
#pragma omp simd
#pragma omp declare simd linear(p:1) inbranch
#pragma omp simd
