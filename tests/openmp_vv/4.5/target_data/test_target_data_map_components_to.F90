!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target data map(to: myStruct) map(to: myStructArr(:))
!$omp               target map(alloc: myStruct, myStructArr) map(tofrom:cpyStruct, cpyStructArr(:))
!$omp               end target
!$omp             end target data
