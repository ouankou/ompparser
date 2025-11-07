!$omp    declare mapper(myvec_t :: v) map(v, v%data(1:v%len))
!$omp   target
!$omp   end target
!$omp   declare target
