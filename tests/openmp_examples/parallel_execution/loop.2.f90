!$omp     loop bind(teams)
!$omp       loop
!$omp   teams num_teams(4)
!$omp   end teams
