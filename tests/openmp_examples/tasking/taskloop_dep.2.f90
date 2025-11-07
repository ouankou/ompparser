!$omp    taskloop collapse(2) nogroup
!$omp          task_iteration depend(inout: B(i,j)) depend(in: B(i-1,j),B(i,j-1))
!$omp    end taskloop
!$omp    task depend(in: B(n,n))
!$omp    end task
