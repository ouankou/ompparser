!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp          target data map(to: a, b) map(tofrom: c)
!$omp             target
!$omp             end target
!$omp          end target data
!$omp          target update if(change_flag) to(b(:1024))
!$omp          target
!$omp          end target
