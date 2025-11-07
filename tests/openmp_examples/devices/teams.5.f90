!$omp    target teams map(to: v1, v2) map(from: p)
!$omp       distribute simd
!$omp    end target teams
