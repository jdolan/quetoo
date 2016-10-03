$QUETOO_PRIVATE_KEY = ${env:QUETOO_PRIVATE_KEY}.Replace("@", "`r`n")
[System.IO.File]::WriteAllText("quetoo.rsa", $QUETOO_PRIVATE_KEY)
chmod 0600 quetoo.rsa

$env:Path += ";..\mingw-cross\Quetoo-x86_64\bin\"

$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$QUETOO_HOST = "$QUETOO_ARCH-MSVC"
$QUETOO_DATA = "../../quetoo-data"
$QUETOO_TARGET = "."
$QUETOO_INSTALL = "$QUETOO_TARGET/Quetoo"
$QUETOO_BINDIR = "$QUETOO_INSTALL/bin"
$QUETOO_ETCDIR = "$QUETOO_INSTALL/etc"
$QUETOO_LIBDIR = "$QUETOO_INSTALL/lib"
$QUETOO_DATADIR = "$QUETOO_INSTALL/share"
$QUETOO_REMOTE_USER = $env:QUETOO_PRIVATE_REMOTE_USER
$QUETOO_RSYNC_REPOSITORY = "quetoo.org:/opt/rsync/quetoo-msvc/$QUETOO_ARCH"
$QUETOO_RSYNC_TARGET = "$QUETOO_REMOTE_USER@$QUETOO_RSYNC_REPOSITORY"
$QUETOO_DIST = "$QUETOO_TARGET/Quetoo-BETA-$QUETOO_HOST.zip"
$QUETOO_HTTP_REPOSITORY = "quetoo.org:/var/www/quetoo.org/files"
$QUETOO_HTTP_TARGET = "$QUETOO_REMOTE_USER@$QUETOO_HTTP_REPOSITORY"

$QUETOO_ARTIFACT_URL = "https://ci.appveyor.com/api/projects/Paril/quetoo/artifacts/Quetoo-BETA-MSVC.zip?job=Platform%3A+$env:Platform"
$QUETOO_WEB_ARTIFACT = "/var/www/quetoo.org/files/Quetoo-BETA-$QUETOO_HOST.zip"

$PIECE_OF_SHIT = "cd /opt/rsync/quetoo-msvc/$QUETOO_ARCH &&
wget -O $QUETOO_WEB_ARTIFACT $QUETOO_ARTIFACT_URL &&
rm -rf bin etc lib &&
unzip -o $QUETOO_WEB_ARTIFACT &&
cp -r Quetoo/{bin,etc,lib} . &&
rm -rf Quetoo".Replace("`r", "")

ssh -i quetoo.rsa ${QUETOO_REMOTE_USER}@quetoo.org $PIECE_OF_SHIT

Pause