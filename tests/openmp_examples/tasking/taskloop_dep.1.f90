!$omp    taskloop nogroup
!$omp       task_iteration depend(inout: A(i)) depend(in: A(i-1))
!$omp    end taskloop
!$omp    taskloop grainsize(strict: 4) nogroup
!$omp       task_iteration depend(inout: A(i)) depend(in: A(i-4)) if(mod(i, 4) == 1 .or. i == n)
!$omp    end taskloop
!$omp    task depend(in: A(n))
!$omp    end task
