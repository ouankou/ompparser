!$omp     target parallel num_threads(8                       ) map(tofrom: total, a, b, num_threads)
!$omp     loop reduction(+:total)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end target parallel
