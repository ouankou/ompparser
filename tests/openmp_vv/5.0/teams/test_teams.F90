!$omp     teams num_teams(8                     ) thread_limit(8)
!$omp       parallel master
!$omp       end parallel master
!$omp     end teams
