!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target data map(tofrom: myStruct, cpyStruct) map(tofrom: myStructArr(:), cpyStructArr(:))
!$omp               target map(alloc: myStruct, cpyStruct) map(alloc:myStructArr(:), cpyStructArr(:))
!$omp               end target
!$omp             end target data
