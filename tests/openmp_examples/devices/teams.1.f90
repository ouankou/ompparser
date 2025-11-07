!$omp     target map(to: B, C) map(tofrom: sum0, sum1)
!$omp     teams num_teams(2)
!$omp          parallel do reduction(+:sum0)
!$omp          parallel do reduction(+:sum1)
!$omp     end teams
!$omp     end target
