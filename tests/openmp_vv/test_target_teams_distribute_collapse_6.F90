!$omp             target teams distribute map(to: a(1:128, 1:128, 1:128)) map(tofrom: b(1:128, 1:128, 1:128+1)) collapse(2) num_teams(8)
