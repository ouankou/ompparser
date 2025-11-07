!$omp declare simd(add1) uniform(fact)
!$omp declare simd(add2) uniform(a,b,fact) linear(i:1)
!$omp    simd private(tmp)
