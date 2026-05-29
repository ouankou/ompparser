!$omp     parallel num_threads(8                     )
!$omp     loop reduction(ieor:b)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end parallel
