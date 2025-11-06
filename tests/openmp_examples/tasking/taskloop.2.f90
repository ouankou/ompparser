!$omp     parallel shared(x1,x2) num_threads(T)
!$omp     taskloop
!$omp         atomic
!$omp         end atomic
!$omp     end taskloop
!$omp     single
!$omp     taskloop
!$omp         atomic
!$omp         end atomic
!$omp     end taskloop
!$omp     end single
!$omp     end parallel
