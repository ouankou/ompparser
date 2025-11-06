!$omp    target if(N>THRESHHOLD1) map(to: v1, v2 ) map(from: p)
!$omp       parallel do if(N>THRESHOLD2)
!$omp    end target
