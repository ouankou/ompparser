!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp       target data map(to: a, b) map(from: c)
!$omp          target
!$omp          end target
!$omp       end target data
!$omp       target update to(b)
!$omp       target
!$omp       end target
