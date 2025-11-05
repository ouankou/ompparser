!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel do order(concurrent) num_threads(8) shared(x, y, z)
!$omp     end parallel do
