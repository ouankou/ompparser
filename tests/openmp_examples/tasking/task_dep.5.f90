!$omp task shared(A,B,C) private(ii,jj,kk) depend(in: A(i:i+BM, k:k+BM),B(k:k+BM, j:j+BM)) depend(inout: C(i:i+BM, j:j+BM))
!$omp end task
