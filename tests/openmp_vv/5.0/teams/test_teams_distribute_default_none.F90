!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     teams distribute num_teams(8                     ) default(none) shared(a,b,c,d,num_teams) private(privatized)
!$omp      teams distribute num_teams(8                     ) default(none) shared(share,g,num_teams)
!$omp         atomic update
!$omp         end atomic
