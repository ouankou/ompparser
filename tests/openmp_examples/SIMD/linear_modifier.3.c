#pragma omp declare simd simdlen(4) uniform(x, y) linear(i:val,step(1))
#pragma omp simd linear(k)
