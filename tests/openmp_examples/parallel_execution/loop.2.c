#pragma omp loop bind(teams)
#pragma omp loop
#pragma omp teams num_teams(4)
