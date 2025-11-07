#pragma omp target data map(a[:n]) use_device_ptr(a)
#pragma omp target parallel for
#pragma omp target parallel for
#pragma omp target teams loop
#pragma omp declare variant(kernel_target_ua) match(implementation={requires(unified_address)})
#pragma omp declare variant(kernel_target_usm) match(implementation={requires(unified_shared_memory)})
#pragma omp declare variant(kernel_target_usm_v2) match(implementation={requires(unified_shared_memory)}, user={condition(score(1): version==2)})
#pragma omp parallel for
