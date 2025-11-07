!$omp    target map(to: B, C) map(tofrom: sum)
!$omp    teams num_teams(8) thread_limit(16) reduction(+:sum)
!$omp    distribute parallel do reduction(+:sum) dist_schedule(static, 1024) schedule(static, 64)
!$omp    end teams
!$omp    end target
