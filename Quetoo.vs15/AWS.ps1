$QUETOO_ARCH = If ($env:Platform -Match "Win32") {"i686"} Else {"x86_64"}
$QUETOO_DEPLOY = "Quetoo " + $env:Platform
$QUETOO_DIST_NAME = "Quetoo-BETA-" + $QUETOO_ARCH + "-MSVS.zip"

Push-AppveyorArtifact "..\Quetoo-BETA-MSVC.zip" -FileName $QUETOO_DIST_NAME -DeploymentName "Quetoo Dist"

$root = Resolve-Path ".\Quetoo\";
[IO.Directory]::GetFiles($root.Path, '*.*', 'AllDirectories') | % { Push-AppveyorArtifact $_ -FileName ("Quetoo\" + $_.Substring($root.Path.Length)) -DeploymentName $QUETOO_DEPLOY }