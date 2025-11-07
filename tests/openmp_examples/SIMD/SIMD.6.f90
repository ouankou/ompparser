!$omp declare simd(foo) notinbranch
!$omp   simd
!$omp declare simd(goo) inbranch
!$omp   simd
