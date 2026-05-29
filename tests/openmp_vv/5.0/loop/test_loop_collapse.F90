!$omp     parallel num_threads(8                     )
!$omp     loop collapse(1)
!$omp     end loop
!$omp     end parallel
!$omp     parallel num_threads(8                     )
!$omp     loop collapse(2)
!$omp     end loop
!$omp     end parallel
