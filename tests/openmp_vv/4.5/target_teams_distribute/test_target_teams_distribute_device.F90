!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp        target enter data map(to: a(1:1024), b(1:1024), c(1:1024, dev:dev), num_teams(dev:dev)) device(dev - 1)
!$omp        target teams distribute map(alloc: a(1:1024), b(1:1024), c(1:1024, dev:dev), num_teams(dev:dev)) device(dev - 1)
!$omp        target exit data map(from: c(1:1024, dev:dev), num_teams(dev:dev)) map(delete: a(1:1024), b(1:1024)) device(dev - 1)
