!$omp   parallel single
!$omp       task depend(inout:h) transparent(omp_impex)
!$omp       end task
!$omp   end parallel single
!$omp       task depend(inout:M(i*N_COLS))
!$omp       end task
