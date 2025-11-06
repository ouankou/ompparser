!$omp    parallel
!$omp       masked
!$omp       target teams distribute parallel do nowait map(to: v1(1:n/2)) map(to: v2(1:n/2)) map(from: vxv(1:n/2))
!$omp       end masked
!$omp       do schedule(dynamic,chunk)
!$omp    end parallel
