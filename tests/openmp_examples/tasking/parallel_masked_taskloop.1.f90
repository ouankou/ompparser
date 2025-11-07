!$omp    parallel
!$omp    masked
!$omp    taskloop
!$omp    end taskloop
!$omp    end masked
!$omp    end parallel
!$omp    parallel masked taskloop
!$omp    end parallel masked taskloop
!$omp    parallel masked taskloop simd
!$omp    end parallel masked taskloop simd
