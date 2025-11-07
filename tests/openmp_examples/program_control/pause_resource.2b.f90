!$omp parallel do private(r) reduction(+:s)
!$omp end parallel do
