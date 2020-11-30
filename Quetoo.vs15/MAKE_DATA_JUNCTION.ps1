$Quetoo_Path = [Environment]::GetEnvironmentVariable("QUETOO_HOME", "User")

if ($Quetoo_Path.length -gt 1) {
    Write-Output "QUETOO_HOME has been set."
	$share = "{0}\share" -f $Quetoo_Path
	if (!(Test-Path $share)) {
        Write "Creating \share directory."
		New-Item -Path $share -ItemType Directory
	}
    $path = "{0}\share\default" -f $Quetoo_Path
    cmd /c ('MKLINK /J "{0}" "..\..\quetoo-data\target\default"' -f $path)
} else {
    Write-Output "QUETOO_HOME is not set yet. Please run SET_ENV.ps1 first."
}

Write-Host "Press any key to continue ..."
$x = $host.UI.RawUI.ReadKey("NoEcho, IncludeKeyDown")