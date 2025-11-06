!$omp    target data map(sum1, sum2)
!$omp        target teams distribute reduction(+:sum1)
!$omp        target teams distribute map(sum1) reduction(+:sum2)
!$omp    end target data
!$omp    declare target enter(f)
!$omp    declare target enter(g)
