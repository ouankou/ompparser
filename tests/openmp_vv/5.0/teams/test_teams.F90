!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     teams num_teams(8                     ) thread_limit(8)
!$omp       parallel master
!$omp       end parallel master
!$omp     end teams
