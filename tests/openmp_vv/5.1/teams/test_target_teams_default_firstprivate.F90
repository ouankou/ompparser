!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams default(firstprivate) map(tofrom: num_teams,errors) shared(num_teams,errors) num_teams(8)
!$omp     end target teams
