!$omp        target parallel num_threads(8                       ) map(tofrom: test_result, a, num_threads)
!$omp        loop reduction(.and.:test_result)
!$omp        end loop
!$omp        do
!$omp        end do
!$omp        end target parallel
