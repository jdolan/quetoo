$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$QUETOO_BUCKET = "s3://quetoo/"

$QUETOO_RELEASE_SRC = "Quetoo/";
$QUETOO_RELEASE_BUCKET = $QUETOO_BUCKET + $QUETOO_ARCH + "-pc-windows"

$QUETOO_SNAPSHOT_SRC = "../Quetoo.zip"
$QUETOO_SNAPSHOT_BUCKET = $QUETOO_BUCKET + "snapshots/Quetoo-BETA-" + $QUETOO_ARCH + "-pc-windows.zip"

$aws_exe = "aws.exe"

&$aws_exe s3 sync $QUETOO_RELEASE_SRC $QUETOO_RELEASE_BUCKET --delete
&$aws_exe s3 cp $QUETOO_SNAPSHOT_SRC $QUETOO_SNAPSHOT_BUCKET

$QUETOO_REVISION_SRC = "revision"
$QUETOO_REVISION_BUCKET = $QUETOO_BUCKET + "revisions/" + $QUETOO_ARCH + "-pc-windows"

&$aws_exe s3 cp $QUETOO_REVISION_SRC $QUETOO_REVISION_BUCKET