!$omp        target teams distribute num_teams(default_num_teams / 2) map(to: a(1:1024), b(1:1024)) map(from: c(1:1024), num_team(1:1024))
