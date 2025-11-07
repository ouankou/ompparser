!$omp   assumes no_parallelism
!$omp    target teams distribute parallel do map(tofrom: A)
!$omp    assume holds (8*(N/8) == N .and. N>0)
!$omp    simd
!$omp    end assume
