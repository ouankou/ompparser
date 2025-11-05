!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp         declare mapper(newvec :: v) map(v, v%data(1:v%len))
!$omp         target
!$omp         end target
!$omp     declare target
