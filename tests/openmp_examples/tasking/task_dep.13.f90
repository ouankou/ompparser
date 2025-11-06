!$omp   parallel masked  num_threads(5)
!$omp     task
!$omp     end task
!$omp     task depend(out: a)
!$omp     end task
!$omp     task depend(out: d)
!$omp     end task
!$omp     task depend(inout: omp_all_memory)
!$omp     end task
!$omp     task
!$omp     end task
!$omp     task depend(in: a,d)
!$omp     end task
!$omp   end parallel masked
