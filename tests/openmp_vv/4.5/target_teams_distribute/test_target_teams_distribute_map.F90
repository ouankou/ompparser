!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target enter data map(alloc: b(1:1024))
!$omp     target teams distribute map(to: a(1:1024), b(1:1024))
!$omp     target exit data map(from: b(1:1024))
!$omp     target enter data map(to: a(1:1024))
!$omp     target teams distribute map(from: b(1:1024))
!$omp     target exit data map(delete: a(1:1024))
!$omp     target enter data map(to: a(1:1024)) map(alloc: b(1:1024))
!$omp     target teams distribute map(alloc: a(1:1024), b(1:1024), c(1:1024))
!$omp     target exit data map(delete: a(1:1024)) map(from: b(1:1024))
!$omp     target teams distribute map(tofrom: a(1:1024), b(1:1024))
