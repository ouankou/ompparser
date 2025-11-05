!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target map(tofrom: data1, data2)
!$omp     parallel do collapse(2)
!$omp     end parallel do
!$omp     end target
