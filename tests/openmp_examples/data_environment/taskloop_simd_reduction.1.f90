!$omp   parallel masked
!$omp   taskloop reduction(+:asum)
!$omp   end taskloop
!$omp   end parallel masked
!$omp   parallel reduction(task, +:asum)
!$omp      masked
!$omp      task            in_reduction(+:asum)
!$omp      end task
!$omp      end masked
!$omp      masked taskloop in_reduction(+:asum)
!$omp      end masked taskloop
!$omp   end parallel
!$omp   parallel masked
!$omp   taskloop simd reduction(+:asum)
!$omp   end taskloop simd
!$omp   end parallel masked
!$omp   parallel reduction(task, +:asum)
!$omp     masked
!$omp     task                 in_reduction(+:asum)
!$omp     end task
!$omp     end masked
!$omp     masked taskloop simd in_reduction(+:asum)
!$omp     end masked taskloop simd
!$omp   end parallel
