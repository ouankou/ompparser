!$omp    target teams distribute reduction(+:sum1)
!$omp    target teams distribute reduction(+:sum2)
!$omp    declare target enter(f)
!$omp    declare target enter(g)
