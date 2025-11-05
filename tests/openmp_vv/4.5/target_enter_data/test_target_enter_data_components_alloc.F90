!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target enter data map(alloc: myStruct, myStructArr(:))
!$omp             target map(alloc: myStruct, myStructArr(:))
!$omp             end target
!$omp             target map(alloc: myStruct, myStructArr(:)) map(from:cpyStruct, cpyStructArr(:))
!$omp             end target
!$omp             target exit data map(delete: myStruct, myStructArr(:))
