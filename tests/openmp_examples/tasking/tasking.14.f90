!$omp task if(.FALSE.)
!$omp task
!$omp     task
!$omp     end task
!$omp end task
!$omp end task
!$omp task final(.TRUE.)
!$omp task
!$omp     task
!$omp     end task
!$omp end task
!$omp end task
