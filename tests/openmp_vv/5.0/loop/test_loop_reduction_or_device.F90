!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp        target parallel num_threads(8                       ) map(tofrom: test_result, a, num_threads)
!$omp        loop reduction(.or.:test_result)
!$omp        end loop
!$omp        do
!$omp        end do
!$omp        end target parallel
