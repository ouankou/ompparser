!$omp        parallel num_threads(8                     )
!$omp        loop reduction(.and.:test_result)
!$omp        end loop
!$omp        do
!$omp        end do
!$omp        end parallel
