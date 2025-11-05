!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp          target teams distribute parallel do map(tofrom: num_teams) num_teams(tested_num_teams(nt))
