!$omp    parallel num_threads(n_procs) proc_bind(close)
!$omp    end parallel
!$omp    parallel num_threads(n_sockets) private(socket_num) proc_bind(spread)
!$omp    end parallel
