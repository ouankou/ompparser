!$omp    target teams map(to: B, C) defaultmap(tofrom:scalar) reduction(+:sum)
!$omp    distribute parallel do reduction(+:sum)
!$omp    end target teams
