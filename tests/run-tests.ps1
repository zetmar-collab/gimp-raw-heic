# Runs every batch test inside gimp-console and prints the result files.
#   .\tests\run-tests.ps1                # all tests
#   .\tests\run-tests.ps1 load heic      # selected tests
# Needs: plug-in installed (.\build.ps1 -Install) and samples
# (tests/fetch-samples.sh, python tests/make_heic_samples.py).
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Only = @())
$ErrorActionPreference = "Continue"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$gc = Get-ChildItem "$env:LOCALAPPDATA\Programs\GIMP 3\bin\gimp-console-3.*.exe" |
      Where-Object Name -match 'console-3\.\d+\.exe$' | Select-Object -First 1
if (-not $gc) { throw "gimp-console not found" }
New-Item -ItemType Directory -Force (Join-Path $root "build\test-out") | Out-Null
$tests = [ordered]@{
  load      = @("test_load.py",      "results.txt")
  compare   = @("compare_ref.py",    "compare.txt")
  robust    = @("test_robust.py",    "robust.txt")
  heic      = @("test_heic.py",      "heic.txt")
  redevelop = @("test_redevelop.py", "redevelop.txt")
  metadata  = @("test_metadata.py",  "metadata.txt")
  options   = @("test_options.py",   "options.txt")
}
foreach ($name in $tests.Keys) {
  if ($Only.Count -and -not ($Only -contains $name)) { continue }
  $script, $out = $tests[$name]
  Write-Host "=== $name ===" -ForegroundColor Cyan
  if ($name -eq "compare" -and -not (Test-Path "build\ref")) {
    Write-Host "skipped: generate references with dcraw_emu first (see README)"; continue
  }
  & $gc.FullName -i --batch-interpreter=python-fu-eval `
     -b "exec(open(r'tests/$script', encoding='utf-8').read())" --quit 2>&1 |
     Select-String "Traceback|Error:" | ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
  Get-Content (Join-Path $root "build\test-out\$out") -ErrorAction SilentlyContinue
}
