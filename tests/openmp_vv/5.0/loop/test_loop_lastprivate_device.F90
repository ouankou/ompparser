!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target parallel num_threads(8                       ) map(tofrom: a, b, x)
!$omp     loop lastprivate(x)
!$omp     end loop
!$omp     end target parallel
!$omp     target parallel num_threads(8                       ) map(tofrom: a, b, x, y)
!$omp     loop lastprivate(x, y) collapse(2)
!$omp     end loop
!$omp     end target parallel
