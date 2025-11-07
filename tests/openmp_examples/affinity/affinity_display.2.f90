!$omp    parallel num_threads(n_sockets) private(socket_num)
!$omp    end parallel
!$omp    parallel num_threads(n_thrds)
!$omp    end parallel
