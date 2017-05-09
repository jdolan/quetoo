Write-Output ("Current QUETOO_HOME: {0}" -f [Environment]::GetEnvironmentVariable('QUETOO_HOME', 'User'))
$Quetoo_Path = Read-Host -Prompt "Input path to Quetoo (the folder that has bin, share, etc) or leave empty to unset`n"

if ($Quetoo_Path.length -gt 1) {
	try {
		$Quetoo_Path = [System.IO.Path]::GetDirectoryName($Quetoo_Path)
		[Environment]::SetEnvironmentVariable("QUETOO_HOME", $Quetoo_Path, "User")
		Write-Output ("QUETOO_HOME is now set to {0}" -f $Quetoo_Path)
	} catch {
		[Environment]::SetEnvironmentVariable("QUETOO_HOME", $null, "User")
		Write-Error "Invalid path!"
	}
} else {
	[Environment]::SetEnvironmentVariable("QUETOO_HOME", $null, "User")
	Write-Output "QUETOO_HOME is now gone."
}

Write-Output ("Current OPENAL_HOME: {0}" -f [Environment]::GetEnvironmentVariable('OPENAL_HOME', 'User'))
$AL_Path = Read-Host -Prompt "Input path to OpenAL SDK (the folder that has include, libs, etc) or leave empty to unset`n"

if ($AL_Path.length -gt 1) {
	try {
		$AL_Path = [System.IO.Path]::GetDirectoryName($AL_Path)
		[Environment]::SetEnvironmentVariable("OPENAL_HOME", $AL_Path, "User")
		Write-Output ("OPENAL_HOME is now set to {0}" -f $AL_Path)
	} catch {
		[Environment]::SetEnvironmentVariable("OPENAL_HOME", $null, "User")
		Write-Error "Invalid path!"
	}
} else {
	[Environment]::SetEnvironmentVariable("OPENAL_HOME", $null, "User")
	Write-Output "OPENAL_HOME is now gone."
}

Write-Host "Press any key to continue ..."
$x = $host.UI.RawUI.ReadKey("NoEcho, IncludeKeyDown")