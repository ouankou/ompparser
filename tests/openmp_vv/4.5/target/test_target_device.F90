!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp               target map(tofrom: array(1:1000)) device(dev)
!$omp               end target
