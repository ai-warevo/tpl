<#
.SYNOPSIS
    Cut a SemVer release from CHANGELOG.md (Windows; Linux/macOS equivalent: scripts/release.sh).

.DESCRIPTION
    Reads "## [Unreleased]" in CHANGELOG.md and picks the next vX.Y.Z from the change categories that have entries:
      Breaking, Removed          -> major
      Added, Deprecated          -> minor
      Fixed, Changed, Security   -> patch
    Then promotes [Unreleased] to "## [X.Y.Z] - <today>", updates the compare links, writes X.Y.Z to VERSION (read by
    CMakeLists.txt), commits CHANGELOG.md + VERSION as "release vX.Y.Z" and creates the annotated tag vX.Y.Z. Pushing the tag triggers the GitHub release workflow.

.PARAMETER Push
    Also push the current branch and the new tag (atomically) to $env:RELEASE_REMOTE (default: origin).

.PARAMETER DryRun
    Only print the detected categories and the next version; change nothing.

.EXAMPLE
    ./scripts/release.ps1 -DryRun
    ./scripts/release.ps1 -Push
#>
[CmdletBinding()]
param(
    [switch]$Push,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Fail([string]$Message) {
    [Console]::Error.WriteLine("error: $Message")
    exit 1
}

# Native commands do not honour $ErrorActionPreference; check the exit code explicitly.
function Invoke-Git {
    & git @args
    if ($LASTEXITCODE -ne 0) { Fail "git $($args -join ' ') failed (exit code $LASTEXITCODE)" }
}

function Get-GitOutput {
    $out = & git @args
    if ($LASTEXITCODE -ne 0) { Fail "git $($args -join ' ') failed (exit code $LASTEXITCODE)" }
    return $out
}

$root = (Get-GitOutput rev-parse --show-toplevel | Select-Object -First 1)
Set-Location $root

$changelog = 'CHANGELOG.md'
$versionFile = 'VERSION'
$remote = if ($env:RELEASE_REMOTE) { $env:RELEASE_REMOTE } else { 'origin' }
if (-not (Test-Path -LiteralPath $changelog)) { Fail "$changelog not found" }

$raw = [System.IO.File]::ReadAllText((Join-Path $root $changelog))
$crlf = $raw.Contains("`r`n")
$lines = [System.Collections.Generic.List[string]]::new()
$lines.AddRange([string[]]($raw -split "\r?\n"))
if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }

$start = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^## \[Unreleased\]\s*$') { $start = $i; break }
}
if ($start -lt 0) { Fail "no '## [Unreleased]' heading in $changelog" }

$end = $lines.Count
for ($i = $start + 1; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^## \[' -or $lines[$i] -match '^\[[^\]]+\]:') { $end = $i; break }
}

# Split the [Unreleased] body into ### categories; "(none)" collects entries that sit outside any category.
$categories = [System.Collections.Generic.List[object]]::new()
$current = $null
for ($i = $start + 1; $i -lt $end; $i++) {
    $line = $lines[$i]
    if ($line -match '^###\s+(.+?)\s*$') {
        $current = [pscustomobject]@{ Heading = $line; Name = $Matches[1]; Body = [System.Collections.Generic.List[string]]::new() }
        $categories.Add($current)
    } elseif ($line -match '^\s*$' -or $line -match '^<!--') {
        if ($null -ne $current) { $current.Body.Add($line) }
    } else {
        if ($null -eq $current) {
            $current = [pscustomobject]@{ Heading = $null; Name = '(none)'; Body = [System.Collections.Generic.List[string]]::new() }
            $categories.Add($current)
        }
        $current.Body.Add($line)
    }
}

function Test-HasEntries($category) {
    foreach ($l in $category.Body) { if ($l -notmatch '^\s*$' -and $l -notmatch '^<!--') { return $true } }
    return $false
}

$filled = @($categories | Where-Object { Test-HasEntries $_ })
if ($filled.Count -eq 0) { Fail '[Unreleased] has no entries; add some under a ### category first' }

$rank = @{ patch = 1; minor = 2; major = 3 }
$bump = 'patch'
$bumpRank = 0
foreach ($category in $filled) {
    switch -CaseSensitive ($category.Name) {
        { $_ -in 'Breaking', 'Removed' } { $level = 'major'; break }
        { $_ -in 'Added', 'Deprecated' } { $level = 'minor'; break }
        { $_ -in 'Fixed', 'Changed', 'Security' } { $level = 'patch'; break }
        '(none)' { Fail 'entries in [Unreleased] must be placed under a ### category' }
        default { Fail "unknown category '### $($category.Name)' in [Unreleased] (use Breaking, Added, Changed, Deprecated, Removed, Fixed, Security)" }
    }
    if ($rank[$level] -gt $bumpRank) { $bumpRank = $rank[$level]; $bump = $level }
}

# Current version = highest of the latest vX.Y.Z tag and the newest "## [X.Y.Z]" heading.
$found = [System.Collections.Generic.List[version]]::new()
foreach ($t in @(Get-GitOutput tag --list 'v[0-9]*.[0-9]*.[0-9]*')) {
    if ($t -match '^v(\d+\.\d+\.\d+)$') { $found.Add([version]$Matches[1]) }
}
foreach ($l in $lines) {
    if ($l -match '^## \[(\d+\.\d+\.\d+)\]') { $found.Add([version]$Matches[1]) }
}
$hasPrevious = $found.Count -gt 0
$currentVersion = if ($hasPrevious) { ($found | Sort-Object | Select-Object -Last 1) } else { [version]'0.0.0' }

$major = $currentVersion.Major; $minor = $currentVersion.Minor; $patch = $currentVersion.Build
switch ($bump) {
    'major' { $major++; $minor = 0; $patch = 0 }
    'minor' { $minor++; $patch = 0 }
    'patch' { $patch++ }
}
$version = "$major.$minor.$patch"
$tag = "v$version"
$previousTag = "v$currentVersion"
$today = Get-Date -Format 'yyyy-MM-dd'

Write-Host "Categories : $(($filled | ForEach-Object { $_.Name }) -join ', ')"
Write-Host "Release    : $previousTag -> $tag ($bump), $today"

if ($DryRun) {
    Write-Host 'Dry run: nothing changed.'
    exit 0
}

& git rev-parse -q --verify "refs/tags/$tag" 2>$null | Out-Null
if ($LASTEXITCODE -eq 0) { Fail "tag $tag already exists" }

& git diff --quiet --cached
if ($LASTEXITCODE -ne 0) { Fail 'the index has staged changes; commit or unstage them first' }

$others = @(Get-GitOutput status --porcelain --untracked-files=no | Where-Object { $_.Substring(3).Trim('"') -notin @($changelog, $versionFile) })
if ($others.Count -gt 0) { Fail "working tree has uncommitted changes besides $changelog and ${versionFile}:`n$($others -join "`n")" }

# Build the promoted section: "## [X.Y.Z] - date" followed by every non-empty category.
$section = [System.Collections.Generic.List[string]]::new()
$section.Add("## [$version] - $today")
$section.Add('')
foreach ($category in $categories) {
    $body = [System.Collections.Generic.List[string]]::new($category.Body)
    while ($body.Count -gt 0 -and $body[0] -match '^\s*$') { $body.RemoveAt(0) }
    while ($body.Count -gt 0 -and $body[$body.Count - 1] -match '^\s*$') { $body.RemoveAt($body.Count - 1) }
    if ($body.Count -eq 0 -or $null -eq $category.Heading) { continue }
    $section.Add($category.Heading)
    $section.Add('')
    $section.AddRange($body)
    $section.Add('')
}

$result = [System.Collections.Generic.List[string]]::new()
$result.AddRange($lines.GetRange(0, $start + 1))
$result.Add('')
$result.AddRange($section)
$result.AddRange($lines.GetRange($end, $lines.Count - $end))

for ($i = 0; $i -lt $result.Count; $i++) {
    if ($result[$i] -match '^\[Unreleased\]:\s+(.*?)/compare/') {
        $base = $Matches[1]
        $newLink = if ($hasPrevious) { "[$version]: $base/compare/$previousTag...$tag" } else { "[$version]: $base/releases/tag/$tag" }
        $result[$i] = "[Unreleased]: $base/compare/$tag...HEAD"
        $result.Insert($i + 1, $newLink)
        break
    }
}

$newline = if ($crlf) { "`r`n" } else { "`n" }
$text = ($result -join $newline) + $newline
[System.IO.File]::WriteAllText((Join-Path $root $changelog), $text, (New-Object System.Text.UTF8Encoding($false)))

[System.IO.File]::WriteAllText((Join-Path $root $versionFile), "$version`n", (New-Object System.Text.UTF8Encoding($false)))

Invoke-Git add -- $changelog $versionFile
Invoke-Git commit -m "release $tag" -- $changelog $versionFile
Invoke-Git tag -a $tag -m "release $tag"
Write-Host "Created commit and annotated tag $tag."

if ($Push) {
    Invoke-Git push --atomic $remote HEAD "refs/tags/$tag"
    Write-Host "Pushed HEAD and $tag to $remote."
} else {
    Write-Host "Not pushed. To publish: git push --atomic $remote HEAD refs/tags/$tag   (or rerun with -Push)"
}
