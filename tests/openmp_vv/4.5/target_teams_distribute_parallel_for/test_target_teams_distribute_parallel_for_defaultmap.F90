!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       target teams distribute parallel do defaultmap(tofrom: scalar)
!$omp       target teams distribute parallel do defaultmap (tofrom:scalar)
!$omp       target teams distribute parallel do
!$omp       target teams distribute parallel do
