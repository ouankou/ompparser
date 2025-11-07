!$omp task shared(found) if(level<10)
!$omp atomic write
!$omp end atomic
!$omp cancel taskgroup
!$omp end task
!$omp task shared(found) if(level<10)
!$omp atomic write
!$omp end atomic
!$omp cancel taskgroup
!$omp end task
!$omp taskwait
!$omp parallel shared(found, tree, value)
!$omp masked
!$omp taskgroup
!$omp end taskgroup
!$omp end masked
!$omp end parallel
