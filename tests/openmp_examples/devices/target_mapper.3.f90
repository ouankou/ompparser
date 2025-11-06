!$omp    declare mapper(myvec_t :: v) map(v, v%data(:))
!$omp   declare mapper(mypoints_t :: v) map(v%x, v%x(1)) map(alloc:v%scratch)
!$omp    target map(P)
!$omp    end target
