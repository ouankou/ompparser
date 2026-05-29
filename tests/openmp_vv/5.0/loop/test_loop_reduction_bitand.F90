!$omp     parallel num_threads(8                     )
!$omp     loop reduction(iand:b)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end parallel
