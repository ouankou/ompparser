!$omp               target map(alloc: my1DPtr(10:50-10)) map(alloc: my1DPtr2(10:), my1DPtr3(:50-10)) map(tofrom: myTmpArray)
