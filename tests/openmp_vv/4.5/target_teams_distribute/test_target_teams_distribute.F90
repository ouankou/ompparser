!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams distribute map(tofrom: a(1:1024), num_teams(1:1024)) map(to: b(1:1024))
