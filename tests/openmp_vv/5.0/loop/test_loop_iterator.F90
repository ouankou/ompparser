!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   target map(from: temp)
!$omp   loop
!$omp   end loop
!$omp   end target
