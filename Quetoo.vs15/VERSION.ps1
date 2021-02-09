$commit = git rev-parse --short HEAD
$commit = '"' + $commit + '"'

[IO.File]::WriteAllText("revision", $commit)

$content = [IO.File]::ReadAllText("src/config.h")

if ($content -imatch '#define(?:\s*)REVISION(?:\s*)(?:.*?)\r?\n') {
	$content = $content -ireplace '#define(?:\s*)REVISION(?:\s*)(?:.*?)\r?\n', "#define REVISION $commit`r`n"
} else {
	$content += "`r`n`r`n#define REVISION $commit`r`n`r`n"
}

[IO.File]::WriteAllText("src/config.h", $content)