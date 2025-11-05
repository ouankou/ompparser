!$omp        target teams distribute if(attempt .gt. 70               )map(tofrom: a(1:1024))
