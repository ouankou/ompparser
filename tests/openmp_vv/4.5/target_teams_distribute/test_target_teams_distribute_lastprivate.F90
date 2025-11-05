!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(to: a(1:1024), b(1:1024)) map(tofrom: c(1:1024))
!$omp     target teams distribute lastprivate(privatized) map(alloc: a(1:1024), b(1:1024), c(1:1024)) defaultmap(tofrom:scalar)
!$omp     end target data
!$omp     target data map(to: a(1:1024), b(1:1024), c(1:1024)) map(tofrom: privatized(1:2))
!$omp     target teams distribute lastprivate(privatized) map(alloc: a(1:1024), b(1:1024), c(1:1024))
!$omp     end target data
