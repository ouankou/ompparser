!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target enter data map(to: anArray(1:5000))
!$omp             target map(alloc: anArray(1:5000))
!$omp             end target
!$omp             target map(from: anotherArray(1:5000))
!$omp             end target
!$omp             target exit data map(delete: anArray(1:5000)) device(dev_data)
