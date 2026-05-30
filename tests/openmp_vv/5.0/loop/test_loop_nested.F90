!$omp     teams num_teams(8                     ) thread_limit(8)
!$omp     loop private(j)
!$omp     end loop
!$omp     parallel if(.FALSE.)
!$omp     end parallel
!$omp     end teams
!$omp     parallel num_threads(8                     )
!$omp     loop
!$omp     end loop
!$omp     end parallel
