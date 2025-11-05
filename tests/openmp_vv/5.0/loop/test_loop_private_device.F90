!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   target parallel num_threads(8                       ) map(tofrom: a, b, c, d, num_threads)
!$omp   loop private(privatized)
!$omp   end loop
!$omp   end target parallel
