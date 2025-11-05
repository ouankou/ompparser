!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(from: d(1:1024)) map(to: a(1:1024), b(1:1024),c(1:1024))
!$omp     target teams distribute firstprivate(privatized, privatized_array) map(alloc: a(1:1024), b(1:1024), c(1:1024), d(1:1024)) num_teams(8)
!$omp     end target data
!$omp     target data map(from: d(1:1024)) map(to: a(1:1024), b(1:1024),c(1:1024))
!$omp     target teams distribute firstprivate(privatized_array, privatized) map(alloc: a(1:1024), b(1:1024), c(1:1024), d(1:1024)) num_teams(8)
!$omp     end target data
