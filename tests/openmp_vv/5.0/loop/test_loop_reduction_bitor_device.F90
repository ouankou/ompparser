!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target parallel num_threads(8                       ) map(tofrom: a, b, num_threads)
!$omp     loop reduction(ior:b)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end target parallel
