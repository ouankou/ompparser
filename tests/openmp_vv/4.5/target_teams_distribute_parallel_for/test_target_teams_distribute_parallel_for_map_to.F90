!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp        target teams distribute parallel do map(to: a, b, scalar) map(tofrom: d)
