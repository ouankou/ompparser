!$omp     target teams distribute parallel do simd map(tofrom: x) shared(x) num_teams(8) num_threads(8)
!$omp        atomic update
!$omp     end target teams distribute parallel do simd
