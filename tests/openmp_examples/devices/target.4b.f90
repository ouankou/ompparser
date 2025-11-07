!$omp    target map(to: v1(1:N), v2(:N)) map(from: p(1:N))
!$omp    parallel do
!$omp    end target
