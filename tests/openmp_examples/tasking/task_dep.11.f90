!$omp     parallel
!$omp     single
!$omp             task depend(out: v(i))
!$omp             end task
!$omp         task depend(iterator(it = 1:n), in: v(it))
!$omp         end task
!$omp     end single
!$omp     end parallel
