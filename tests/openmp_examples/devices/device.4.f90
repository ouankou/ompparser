!$omp   target is_device_ptr(cp) device(targ_dev)
!$omp   end target
!$omp    target device(targ_dev) is_device_ptr(cp_dst)
!$omp    end target
!$omp    target device(targ_dev) is_device_ptr(cp_dst)
!$omp    end target
!$omp    target device(targ_dev) map(h_fp)
!$omp    end target
