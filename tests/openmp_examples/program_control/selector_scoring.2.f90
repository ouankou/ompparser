!$omp       declare variant(kernel_target_ua) match(implementation={requires(unified_address)})
!$omp       declare variant(kernel_target_usm) match(implementation={requires(unified_shared_memory)})
!$omp       declare variant(kernel_target_usm_v2) match(implementation={requires(unified_shared_memory)}, user={condition(score(1): version==2)})
!$omp       parallel do
!$omp       target data map(a(:n)) use_device_ptr(c_ap)
!$omp       target
!$omp          parallel do
!$omp       end target
!$omp       end target data
!$omp       target parallel do
!$omp       target teams loop
