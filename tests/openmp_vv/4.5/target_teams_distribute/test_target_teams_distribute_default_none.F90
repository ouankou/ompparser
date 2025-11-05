!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target data map(from: d(1:1024)) map(to: a(1:1024), b(1:1024),c(1:1024))
!$omp     target teams distribute default(none) shared(a, b, c, d) private(x, y, privatized) num_teams(8)
!$omp     end target data
!$omp     target data map(to: a(1:1024)) map(tofrom: share)
!$omp     target teams distribute default(none) private(x) shared(share,a) defaultmap(tofrom:scalar) num_teams(8)
!$omp        atomic
!$omp     end target data
