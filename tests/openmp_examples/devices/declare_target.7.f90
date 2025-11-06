!$omp    declare target enter(foo) device_type(nohost)
!$omp    declare variant(foo_onhost) match(device={kind(host)})
!$omp    target
!$omp    end target
