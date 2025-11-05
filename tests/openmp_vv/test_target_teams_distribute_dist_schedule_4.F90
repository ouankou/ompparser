!$omp     target teams distribute map(from: num_teams) map(tofrom: a(1:1024)) dist_schedule(static, 64)
