!$omp             target simd map(to: b(1:1024      ,1:1024      ), c(1:1024,1:1024)) map(tofrom: a(1:1024,1:1024)) collapse(2)
