$commit = '"' + $env:APPVEYOR_REPO_COMMIT + '"'
$content = [IO.File]::ReadAllText("src/config.h")

if ($content -imatch '#define(?:\s*)COMMIT_ID(?:\s*)(?:.*?)\r?\n')
{
$content = $content -ireplace '#define(?:\s*)COMMIT_ID(?:\s*)(?:.*?)\r?\n', "#define COMMIT_ID $commit`r`n"
}
else
{
$content += "`r`n`r`n#define COMMIT_ID $commit`r`n`r`n"
}

[IO.File]::WriteAllText("src/config.h", $content)