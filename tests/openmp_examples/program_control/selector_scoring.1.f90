!$omp       declare variant(fx1) match(construct={target})
!$omp       declare variant(fx2) match(construct={teams,parallel,do})
!$omp       declare variant(fx3) match(device={kind(gpu),isa(sm_70)})
!$omp       declare variant(fx4) match(device={arch(nvptx),isa(sm_70)})
!$omp    target teams distribute parallel do map(a)
!$omp       task
!$omp       end task
