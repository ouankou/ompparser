!$omp          declare target enter(do_work)
!$omp          declare target enter(other_work)
!$omp    target data map(a)
!$omp       target
!$omp       end target
!$omp       target update from( a(1,1:ny), a(nx,1:ny) )
!$omp       target update to( a(0,1:ny), a(nx+1,1:ny) )
!$omp       target
!$omp       end target
!$omp    end target data
