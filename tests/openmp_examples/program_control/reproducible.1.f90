!$omp    threadprivate(sum)
!$omp    parallel do order(concurrent)
!$omp    parallel do ordered
!$omp       ordered
!$omp       end ordered
!$omp    parallel do copyin(sum)
!$omp    parallel
!$omp    end parallel
