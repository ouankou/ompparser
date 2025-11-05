!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams distribute map(from: num_teams) map(tofrom: a(1:1024)) dist_schedule(static, 64)
!$omp     end target teams distribute
!$omp     target teams distribute map(from: num_teams) map(tofrom: b(1:1024)) dist_schedule(static)
!$omp     end target teams distribute
