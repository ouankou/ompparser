!$omp task shared(x) mergeable
!$omp end task
!$omp taskwait
