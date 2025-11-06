!$omp    target data map(to: v1, v2) map(from: p0)
!$omp    end target data
!$omp    target map(to: v3, v4) map(from: p1)
!$omp    parallel do
!$omp    end target
