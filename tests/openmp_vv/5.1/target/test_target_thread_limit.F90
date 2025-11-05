!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(tofrom: num_teams,errors) thread_limit(testing_thread_limit)
!$omp     teams num_teams(8                     )
!$omp     parallel
!$omp     end parallel
!$omp     end teams
!$omp     end target
