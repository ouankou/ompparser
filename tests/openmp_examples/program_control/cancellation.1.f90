!$omp parallel shared(err)
!$omp do private(s, B)
!$omp cancellation point do
!$omp atomic write
!$omp cancel do
!$omp end parallel
