!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target enter data map(to: scalar_var, A, new_struct, ptr)
!$omp     target map(tofrom: errors) defaultmap(present)
!$omp     end target
!$omp     target exit data map(delete: scalar_var, A, new_struct, ptr)
