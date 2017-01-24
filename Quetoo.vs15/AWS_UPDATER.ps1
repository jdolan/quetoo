$QUETOO_BUCKET = "s3://quetoo/"
$QUETOO_RELEASE_SRC = "Quetoo/";
$QUETOO_UPDATE = "quetoo-update-small.jar"
$QUETOO_LIB_DIR = $QUETOO_RELEASE_SRC + "lib/"

$aws_exe = "aws.exe"

&$aws_exe s3 cp $QUETOO_BUCKET/snapshots/$QUETOO_UPDATE $QUETOO_LIB_DIR