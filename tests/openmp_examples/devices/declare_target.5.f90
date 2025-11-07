!$omp declare target (N,Q)
!$omp declare simd uniform(i) linear(k) notinbranch
!$omp declare target
!$omp    target map(tofrom: tmp)
!$omp    parallel do private(tmp1) reduction(+:tmp)
!$omp       simd reduction(+:tmp1)
!$omp    end target
