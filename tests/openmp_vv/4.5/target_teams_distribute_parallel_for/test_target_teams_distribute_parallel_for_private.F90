!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       target data map(to: a, b, c) map(from: d)
!$omp          target teams distribute parallel do private(privatized, i) num_threads(8) num_teams(8)
!$omp       end target data
