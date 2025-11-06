!$omp   declare target enter(fun1) indirect
!$omp   declare target enter(fun2) indirect
!$omp   target map(from: ret_val)
!$omp   end target
