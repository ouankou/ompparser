!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target enter data map(to: anArray(1:5000)) device(dev_data)
!$omp             target map(alloc: anArray(1:5000)) device(dev_comp)
!$omp             end target
!$omp             target exit data map(from: anArray(1:5000)) device(dev_data)
