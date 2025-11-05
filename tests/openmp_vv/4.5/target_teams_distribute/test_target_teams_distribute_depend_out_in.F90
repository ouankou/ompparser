!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(to: a(1:1024), b(1:1024)) map(alloc: c(1:1024)) map( from: d(1:1024))
!$omp     target teams distribute nowait depend(out: c) map(alloc: b(1:1024), c(1:1024), d(1:1024))
!$omp     target teams distribute nowait depend(in: c) map(alloc: b(1:1024), c(1:1024), d(1:1024))
!$omp     taskwait
!$omp     end target data
!$omp     target data map(to: a(1:1024), b(1:1024)) map(alloc: c(1:1024)) map(from: d(1:1024))
!$omp     target teams distribute nowait depend(inout: c) map(alloc: a(1:1024), b(1:1024), c(1:1024))
!$omp     target teams distribute nowait depend(in: c) map(alloc: a(1:1024), c(1:1024), d(1:1024))
!$omp     taskwait
!$omp     end target data
