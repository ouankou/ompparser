!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       target teams distribute parallel do num_threads(8)
!$omp       parallel do num_threads(8                       )
!$omp          target teams distribute parallel do if(parallel: attempt.gt. 70) map(tofrom: a, warning) num_threads(8)
