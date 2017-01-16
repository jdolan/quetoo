$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$QUETOO_BUCKET = "s3://quetoo/"

$QUETOO_SNAPSHOT_SRC = "Quetoo\";
$QUETOO_SNAPSHOT_BUCKET = $QUETOO_BUCKET + $QUETOO_ARCH + "-msvs"

$QUETOO_DIST_SRC = "..\Quetoo-BETA-MSVC.zip"
$QUETOO_DIST_BUCKET = $QUETOO_BUCKET + "dist/Quetoo-BETA-" + $QUETOO_ARCH + "-MSVS.zip"

$aws_exe = "aws.exe"

&$aws_exe s3 sync $QUETOO_SNAPSHOT_SRC $QUETOO_BUCKET_BUCKET
&$aws_exe s3 cp $QUETOO_DIST_SRC $QUETOO_DIST_BUCKET
