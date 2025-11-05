!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       target teams distribute parallel do map(tofrom: a, b, c, d,scalar_to, scalar_from)
!$omp          atomic write
