$aws_exe = "aws.exe"

$QUETOO_REVISION_SRC = "revision"
$QUETOO_BUCKET = "s3://quetoo/"
$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$QUETOO_REVISION_BUCKET = $QUETOO_BUCKET + "revisions/" + $QUETOO_ARCH + "-pc-windows"

# upload revisions/ file which just contains current commit name
&$aws_exe s3 cp $QUETOO_REVISION_SRC $QUETOO_REVISION_BUCKET

$QUETOO_RELEASE_SRC = "Quetoo/";
$QUETOO_DATA_BUCKET = "s3://quetoo-data/"
$QUETOO_DATA_DIR = $QUETOO_RELEASE_SRC + "share/"

# pull in data into Quetoo/share/
&$aws_exe s3 sync $QUETOO_DATA_BUCKET/ $QUETOO_DATA_DIR