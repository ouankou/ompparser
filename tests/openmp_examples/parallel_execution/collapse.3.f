!$omp parallel num_threads(2)
!$omp do collapse(2) ordered private(j,k) schedule(static,3)
!$omp ordered
!$omp end ordered
!$omp end do
!$omp end parallel
