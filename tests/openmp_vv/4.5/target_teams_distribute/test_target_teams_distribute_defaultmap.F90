!$omp target map(from:ompvv_isHost)
!$omp end target
!$omp target map(to: isSharedProb)
!$omp end target
!$omp     target teams distribute defaultmap(tofrom: scalar) map(tofrom:byte_array(1:1024), short_array(1:1024), int_array(1:1024), long_int_array(1:1024), float_array(1:1024), double_array(1:1024), logical_array(1:1024), logical_kind4_array(1:1024), logical_kind8_array(1:1024), complex_array(1:1024))
!$omp     end target teams distribute
!$omp     target teams distribute defaultmap(tofrom: scalar)
!$omp     end target teams distribute
!$omp     target teams distribute map(tofrom: byte_array_a(1:1024), byte_array_b(1:1024), short_array_a(1:1024), short_array_b(1:1024), int_array_a(1:1024), int_array_b(1:1024), float_array_a(1:1024), float_array_b(1:1024), double_array_a(1:1024), double_array_b(1:1024), logical_array_a(1:1024, 1:16), logical_array_b(1:1024), logical_kind4_array_a(1:1024, 1:16), logical_kind4_array_b(1:1024), logical_kind8_array_a(1:1024, 1:16), logical_kind8_array_b(1:1024), complex_array_a(1:1024), complex_array_b(1:1024))
!$omp     target teams distribute map(tofrom: byte_array_a(1:1024), short_array_a(1:1024), int_array_a(1:1024), long_int_array_a(1:1024),float_array_a(1:1024), double_array_a(1:1024), logical_array_b(1:1024), logical_kind4_array_b(1:1024), logical_kind8_array_b(1:1024), complex_array_a(1:1024))
!$omp     target teams distribute
