!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target teams distribute parallel do map(from:num_teams, num_threads) num_teams(8) num_threads(8)
!$omp               atomic write
