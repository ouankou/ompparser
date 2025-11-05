!$omp        target enter data map(to: a(1:1024), b(1:1024), c(1:1024, dev:dev), num_teams(dev:dev)) device(dev - 1)
