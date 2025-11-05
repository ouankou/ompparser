!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams num_teams(8                     ) thread_limit(8)map(tofrom: x, y, z, num_teams)
!$omp     loop private(j)
!$omp     end loop
!$omp     parallel if(.FALSE.)
!$omp     end parallel
!$omp     end target teams
!$omp     target parallel num_threads(8                     ) map(tofrom: x, y, z, num_threads)
!$omp     loop
!$omp     end loop
!$omp     end target parallel
