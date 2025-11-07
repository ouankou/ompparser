!$omp     target map(to: B, C) map(tofrom: sum)
!$omp     teams num_teams(num_teams) thread_limit(block_threads) reduction(+:sum)
!$omp     distribute
!$omp           parallel do reduction(+:sum)
!$omp     end teams
!$omp     end target
