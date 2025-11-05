!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp                 target enter data map(A, B, D)
!$omp                         target map(tofrom: errors)
!$omp                         end target
!$omp                 target exit data map(A, B, D)
