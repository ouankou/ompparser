!$omp             target enter data map(to: anArray(1:5000))
!$omp             target map(alloc: anArray(1:5000))
!$omp             end target
!$omp             target exit data map(from: anArray(1:5000))
