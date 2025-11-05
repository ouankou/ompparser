!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams distribute parallel do collapse(4) map(tofrom: a)private(i,j,k,l)
