!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp   target teams distribute default(shared) num_teams(8)
!$omp   atomic
!$omp   end atomic
