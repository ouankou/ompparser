!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       target enter data map(to: a, b)
!$omp       target
!$omp       end target
!$omp       target update from(b)
!$omp       target
!$omp       end target
