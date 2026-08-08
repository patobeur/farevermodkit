$files = Get-ChildItem "modules\*.cpp"
foreach ($f in $files) {
    $content = Get-Content $f.FullName -Raw
    $content = $content -replace '#include "\.\./', '#include "'
    [IO.File]::WriteAllText($f.FullName, $content)
}
