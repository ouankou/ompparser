!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     parallel reduction(task, +:test_sum) num_threads(8) shared(y,z, num_threads)
!$omp        master
!$omp        taskloop in_reduction(+:test_sum)
!$omp        end taskloop
!$omp        end master
!$omp     end parallel
