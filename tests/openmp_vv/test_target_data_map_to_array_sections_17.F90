!$omp               target map(alloc: my1DArray(10:50-10)) map(alloc: my1DArray2(10:), my1DArray3(:50-10)) map(tofrom: myTmpArray)
