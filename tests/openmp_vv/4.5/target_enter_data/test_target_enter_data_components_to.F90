!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp             target enter data map(to: myStruct, myStructArr(:))
!$omp             target map(alloc: myStruct, myStructArr) map(tofrom: cpyStruct, cpyStructArr(:))
!$omp             end target
!$omp             target exit data map(delete: myStruct, myStructArr(:))
