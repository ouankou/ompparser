!$omp   requires reverse_offload
!$omp   declare target device_type(host)
!$omp   requires reverse_offload
!$omp    target map(A)
!$omp            target device(ancestor: 1) map(always,to: A(i))
!$omp            end target
!$omp    end target
