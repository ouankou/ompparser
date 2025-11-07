!$omp    target teams map(to: v1, v2) map(from: p)
!$omp       distribute parallel do simd
!$omp    end target teams
