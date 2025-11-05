!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(to: a(1:1024)) map(tofrom: share, num_teams)
!$omp     target teams distribute default(shared) defaultmap(tofrom:scalar) num_teams(8)
!$omp        atomic
!$omp     end target data
!$omp     target data map(tofrom: a(1:1024), share)
!$omp     target teams distribute default(shared) defaultmap(tofrom:scalar) num_teams(8)
!$omp     end target data
