!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target map(myStruct, cpyStruct)
!$omp             end target
!$omp             target map(myStruct, cpyStruct)
!$omp             end target
