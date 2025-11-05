!$omp        target exit data map(from: c(1:1024, dev:dev), num_teams(dev:dev)) map(delete: a(1:1024), b(1:1024)) device(dev - 1)
