!$omp     target parallel num_threads(8                       ) map(tofrom: a, b, num_threads)
!$omp     loop reduction(ior:b)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end target parallel
