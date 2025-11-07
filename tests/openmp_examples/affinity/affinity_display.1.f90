!$omp    parallel num_threads(omp_get_num_procs())
!$omp    end parallel
!$omp    parallel num_threads( omp_get_num_procs() )
!$omp    end parallel
!$omp    parallel num_threads( omp_get_num_procs()/2 )
!$omp    end parallel
