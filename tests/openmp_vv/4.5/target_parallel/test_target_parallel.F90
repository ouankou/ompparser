!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp          target parallel num_threads(8                       ) map(from: summation, thread_id)
!$omp          end target parallel
