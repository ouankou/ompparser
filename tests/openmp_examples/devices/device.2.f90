!$omp    target if(do_offload) map(to: v1, v2) map(from: p)
!$omp    parallel do if(N>1000)
!$omp    end target
