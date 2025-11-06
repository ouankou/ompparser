!$omp        declare variant( p_vxv ) match( construct={parallel} )
!$omp        declare variant( t_vxv ) match( construct={target}   )
!$omp       do
!$omp       declare target
!$omp       distribute simd
!$omp    parallel
!$omp    end parallel
!$omp    target teams map(to: v1,v2) map(from: v3)
!$omp    end target teams
