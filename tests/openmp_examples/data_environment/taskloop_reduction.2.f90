!$omp     taskgroup task_reduction(+: res)
!$omp         task in_reduction(+: res)
!$omp         end task
!$omp         taskloop in_reduction(+: res) nogroup
!$omp         end taskloop
!$omp     end taskgroup
!$omp     parallel
!$omp     single
!$omp     end single
!$omp     end parallel
