!$omp       declare variant( avx512_saxpy ) match(device={isa("core-avx512")})
!$omp       parallel do simd simdlen(16) aligned(x,y: 64)
