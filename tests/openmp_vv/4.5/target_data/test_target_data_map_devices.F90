!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp               target data map(tofrom: anArray(1:5000))
!$omp                   target map(alloc: anArray(1:5000))
!$omp                   end target
!$omp               end target data
!$omp               target data map(tofrom: anArray(1:5000)) device(dev_data)
!$omp                   target map(alloc: anArray(1:5000)) device(dev_comp)
!$omp                   end target
!$omp              end target data
