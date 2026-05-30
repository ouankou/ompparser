!$omp     parallel num_threads(8                     )
!$omp     loop reduction(+:total)
!$omp     end loop
!$omp     do
!$omp     end do
!$omp     end parallel
