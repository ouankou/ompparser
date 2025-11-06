!$omp   declare target
!$omp     metadirective when(construct={target}: distribute parallel do)otherwise(parallel do simd)
!$omp       target teams map(from: d)
!$omp       end target teams
