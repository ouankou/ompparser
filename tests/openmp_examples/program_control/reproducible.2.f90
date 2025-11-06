!$omp    parallel
!$omp       do order(reproducible: concurrent)
!$omp       end do nowait
!$omp       do order(reproducible: concurrent)
!$omp    end parallel
!$omp    parallel
!$omp       do schedule(static) order(concurrent)
!$omp       end do nowait
!$omp       do schedule(static) order(concurrent)
!$omp    end parallel
!$omp    parallel
!$omp       do schedule(static) order(unconstrained: concurrent)
!$omp       do schedule(static) order(unconstrained: concurrent)
!$omp    end parallel
