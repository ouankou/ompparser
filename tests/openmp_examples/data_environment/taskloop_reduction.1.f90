!$omp     taskloop reduction(+: res)
!$omp     end taskloop
!$omp     parallel
!$omp     single
!$omp     end single
!$omp     end parallel
