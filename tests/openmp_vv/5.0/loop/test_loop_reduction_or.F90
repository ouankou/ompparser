!$omp        parallel num_threads(8                     )
!$omp        loop reduction(.or.:test_result)
!$omp        end loop
!$omp        do
!$omp        end do
!$omp        end parallel
