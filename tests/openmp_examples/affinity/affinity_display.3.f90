!$omp    parallel private(thrd_num,nchars) reduction(max:max_req_store)
!$omp    end parallel
