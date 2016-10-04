$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$postParams = @{arch=$QUETOO_ARCH;job_id=$env:APPVEYOR_JOB_ID;secret=$env:QUETOO_SECRET}
Invoke-WebRequest -Uri http://quetoo.org/quetoo-msvc-webhook/ -Method POST -Body $postParams