!$omp    declare target link(sp,sv1,sv2)
!$omp    declare target link(dp,dv1,dv2)
!$omp    declare target
!$omp       parallel do
!$omp    declare target
!$omp       parallel do
!$omp    target map(to:sv1,sv2) map(from:sp)
!$omp    end target
!$omp    target map(to:dv1,dv2) map(from:dp)
!$omp    end target
