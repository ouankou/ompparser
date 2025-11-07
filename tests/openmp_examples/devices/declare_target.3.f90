!$omp declare target (N, p, v1, v2)
!$omp    target update to(v1, v2)
!$omp    target
!$omp    parallel do
!$omp    end target
!$omp    target update from (p)
