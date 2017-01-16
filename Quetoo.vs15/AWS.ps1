$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$QUETOO_SNAPSHOT = "Quetoo\";
$QUETOO_BUCKET = "s3://quetoo/" + $QUETOO_ARCH + "-msvs"
$QUETOO_DIST_SRC = "..\Quetoo-BETA-MSVC.zip"
$QUETOO_DIST = $QUETOO_BUCKET + "/Quetoo-BETA-" + $QUETOO_ARCH + "-MSVS.zip"

$aws_exe = "aws.exe"

&$aws_exe s3 sync $QUETOO_SNAPSHOT $QUETOO_BUCKET
&$aws_exe s3 cp $QUETOO_DIST_SRC $QUETOO_DIST
