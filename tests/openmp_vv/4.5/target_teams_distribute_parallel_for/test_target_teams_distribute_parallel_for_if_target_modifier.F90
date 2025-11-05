!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp          target teams distribute parallel do if(target: attempt .gt. 70) map(tofrom: a) num_threads(8)
