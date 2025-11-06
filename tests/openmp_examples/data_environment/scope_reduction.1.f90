!$omp    do
!$omp    single
!$omp    end single
!$omp    scope reduction(+:s,nthrs)
!$omp    end scope
!$omp    masked
!$omp    end masked
!$omp    parallel
!$omp    end parallel
