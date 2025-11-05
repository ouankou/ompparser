!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       parallel num_threads(8                     )
!$omp       loop reduction(*:device_result)
!$omp       end loop
!$omp       do
!$omp       end do
!$omp       end parallel
