!$omp    target parallel do if(target: N>THRESHHOLD1) if(parallel: N>THRESHOLD2) map(to: v1, v2) map(from: p)
!$omp    end target parallel do
