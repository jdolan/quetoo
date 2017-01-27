$QUETOO_BUCKET = "s3://quetoo/"
$QUETOO_RELEASE_SRC = "Quetoo/";
$QUETOO_UPDATE = "quetoo-installer-small.jar"
$QUETOO_LIB_DIR = $QUETOO_RELEASE_SRC + "lib/"

$aws_exe = "aws.exe"

$QUETOO_UPDATER_JAR = $QUETOO_BUCKET + "snapshots/" + $QUETOO_UPDATE

&$aws_exe s3 cp $QUETOO_UPDATER_JAR $QUETOO_LIB_DIR