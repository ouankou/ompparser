!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target parallel num_threads(8                       ) map(tofrom: a, b)
!$omp     loop collapse(1)
!$omp     end loop
!$omp     end target parallel
!$omp     target parallel num_threads(8                       ) map(tofrom: a, b, num_threads)
!$omp     loop collapse(2)
!$omp     end loop
!$omp     end target parallel
