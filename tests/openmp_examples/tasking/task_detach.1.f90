!$omp   parallel
!$omp   masked
!$omp     task detach(event)
!$omp     end task
!$omp     taskwait
!$omp   end masked
!$omp   end parallel
