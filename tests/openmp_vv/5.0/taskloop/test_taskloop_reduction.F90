!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel num_threads(8                     ) shared(a, b, num_threads, test_sum)
!$omp        single
!$omp        taskloop reduction(+:test_sum)
!$omp           atomic
!$omp           end atomic
!$omp        end taskloop
!$omp        end single
!$omp        single
!$omp        taskloop reduction(+:test_sum)
!$omp           atomic
!$omp           end atomic
!$omp        end taskloop
!$omp        end single
!$omp     end parallel
