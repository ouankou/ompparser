!$omp declare target (N,Q)
!$omp declare target
!$omp    target map(tofrom: tmp)
!$omp    parallel do reduction(+:tmp)
!$omp    end target
