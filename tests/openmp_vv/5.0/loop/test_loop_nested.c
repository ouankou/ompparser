#pragma omp teams num_teams(8) thread_limit(8)
#pragma omp loop
#pragma omp parallel if(0)
#pragma omp parallel num_threads(8)
#pragma omp loop
