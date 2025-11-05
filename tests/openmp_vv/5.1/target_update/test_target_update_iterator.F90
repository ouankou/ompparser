!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp         target enter data map(to: new_struct%data)
!$omp         target map(to: new_struct, new_struct%data) map(tofrom: A)
!$omp         end target
!$omp         target update to(iterator(it = 1:1024): new_struct%data(it))
!$omp         target map(tofrom: A)
!$omp         end target
!$omp         target exit data map(delete: new_struct%data)
