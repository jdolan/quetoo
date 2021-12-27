if (-not $env:APPVEYOR_REPO_BRANCH)
{
	echo "APPVEYOR_REPO_BRANCH is empty ($env:APPVEYOR_REPO_BRANCH)"
	return
}

echo "AWS for $env:APPVEYOR_REPO_BRANCH..."

$IS_MASTER = $env::APPVEYOR_REPO_BRANCH -eq "master"

$aws_exe = "aws.exe"

$QUETOO_REVISION_SRC = "revision"
$QUETOO_BUCKET = "s3://quetoo/"
$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}

if ($IS_MASTER)
{
	$QUETOO_REVISION_BUCKET = $QUETOO_BUCKET + "revisions/" + $QUETOO_ARCH + "-pc-windows"

	# upload revisions/ file which just contains current commit name
	echo "Uploading revisions"
	&$aws_exe s3 cp $QUETOO_REVISION_SRC $QUETOO_REVISION_BUCKET
}

$QUETOO_RELEASE_SRC = "Quetoo/";

if ($IS_MASTER)
{
	$QUETOO_LIB_DIR = $QUETOO_RELEASE_SRC + "lib/"
	$QUETOO_UPDATE = "quetoo-installer-small.jar"
	$QUETOO_UPDATER_JAR = $QUETOO_BUCKET + "snapshots/" + $QUETOO_UPDATE

	# copy updater from s3 to lib
	echo "Copy updater.jar from s3"
	&$aws_exe s3 cp $QUETOO_UPDATER_JAR $QUETOO_LIB_DIR

	$QUETOO_RELEASE_BUCKET = $QUETOO_BUCKET + $QUETOO_ARCH + "-pc-windows"

	# sync from Quetoo/ to quetoo/<arch>/ bucket
	echo "Syncing to release bucket"
	&$aws_exe s3 sync $QUETOO_RELEASE_SRC $QUETOO_RELEASE_BUCKET --delete
}

$QUETOO_DATA_BUCKET = "s3://quetoo-data/"
$QUETOO_DATA_DIR = $QUETOO_RELEASE_SRC + "share/"

# pull in data into Quetoo/share/
echo "Syncing data"
&$aws_exe s3 sync $QUETOO_DATA_BUCKET $QUETOO_DATA_DIR

$QUETOO_SNAPSHOT_SRC = "Quetoo.zip"
if ($IS_MASTER)
{
	$QUETOO_SNAPSHOT_BUCKET = $QUETOO_BUCKET + "snapshots/Quetoo-BETA-" + $QUETOO_ARCH + "-pc-windows.zip"
}
else
{
	$QUETOO_SNAPSHOT_BUCKET = $QUETOO_BUCKET + "snapshots/Quetoo-BETA-" + $QUETOO_ARCH + "-" + $env::APPVEYOR_REPO_BRANCH + "-pc-windows.zip"
}

# zip up Quetoo/
echo "Zipping snapshot"
7z a $QUETOO_SNAPSHOT_SRC Quetoo\

# upload to snapshots/<arch>.zip
echo "Uploading snapshot"
&$aws_exe s3 cp $QUETOO_SNAPSHOT_SRC $QUETOO_SNAPSHOT_BUCKET