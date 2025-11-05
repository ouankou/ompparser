!$omp     target teams distribute map(from: num_teams) map(tofrom: b(1:1024)) dist_schedule(static)
