!$omp task final(pos > LIMIT) mergeable
!$omp end task
!$omp task final(pos > LIMIT) mergeable
!$omp end task
!$omp taskwait
