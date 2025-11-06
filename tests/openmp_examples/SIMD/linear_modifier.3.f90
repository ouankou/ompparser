!$omp declare simd(func) simdlen(4) uniform(x, y) linear(i:val,step(1))
!$omp simd linear(k)
