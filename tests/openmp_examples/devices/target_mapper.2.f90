!$omp     declare mapper( left_id: dzmat_t :: v) map( v%r_m(N,  1:N/2),v%i_m(N, 1:N/2))
!$omp     declare mapper(right_id: dzmat_t :: v) map( v%r_m(N,N/2+1:N),v%i_m(N,N/2+1:N))
!$omp   target map(mapper( left_id), tofrom: a,b) device(0) firstprivate(is,ie) nowait
!$omp   end target
!$omp   target map(mapper(right_id), tofrom: a,b) device(1) firstprivate(is,ie) nowait
!$omp   end target
!$omp   taskwait
