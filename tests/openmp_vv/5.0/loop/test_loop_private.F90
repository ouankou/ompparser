!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   parallel num_threads(8                     )
!$omp   loop private(privatized)
!$omp   end loop
!$omp   end parallel
