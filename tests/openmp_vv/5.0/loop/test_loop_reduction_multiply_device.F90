!$omp       target parallel num_threads(8                       ) map(tofrom: device_result, a, num_threads)
!$omp       loop reduction(*:device_result)
!$omp       end loop
!$omp       do
!$omp       end do
!$omp       end target parallel
