!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(to: a(1:1024), b(1:1024)) map(tofrom: c(1:1024), d(1:1024))
!$omp     target teams distribute nowait depend(in:d) map(alloc: a(1:1024), b(1:1024), d(1:1024))
!$omp        atomic
!$omp     target teams distribute nowait depend(in:d) map(alloc: a(1:1024), b(1:1024), c(1:1024), d(1:1024))
!$omp        atomic
!$omp     taskwait
!$omp     end target data
