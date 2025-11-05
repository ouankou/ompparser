!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target parallel num_threads(8                       ) map(tofrom: device_result, a, b, num_threads)
!$omp     loop reduction(max:device_result)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end target parallel
