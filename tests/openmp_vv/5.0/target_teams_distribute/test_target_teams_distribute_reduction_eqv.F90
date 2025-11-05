!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp        target teams distribute map(to: a(1:1024)) reduction(.eqv.:result) map(tofrom: result)
