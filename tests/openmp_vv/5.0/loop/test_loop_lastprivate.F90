!$omp     parallel num_threads(8                     )
!$omp     loop lastprivate(x)
!$omp     end loop
!$omp     end parallel
!$omp     parallel num_threads(8                     )
!$omp     loop lastprivate(x, y) collapse(2)
!$omp     end loop
!$omp     end parallel
