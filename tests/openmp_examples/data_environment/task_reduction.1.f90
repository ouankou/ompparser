!$omp     taskgroup task_reduction(+: res)
!$omp             task in_reduction(+: res)
!$omp             end task
!$omp     end taskgroup
!$omp     parallel
!$omp     single
!$omp     end single
!$omp     end parallel
