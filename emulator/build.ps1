# == CONFIG ==

$executable_name = "chip8"
$optimization = $true

$SDL_include = "-IC:\dev\SDL2\64\include\SDL2"
$SDL_library = "-LC:\dev\SDL2\64\lib"

# == END CONFIG ==

$old_title = $host.UI.RawUI.WindowTitle
$host.UI.RawUI.WindowTitle = "Building SDL Project"

# get all .c files. Files beginning with '_' are ignored!
$src_files = (Get-ChildItem "*.c" -Recurse) | ? { $_.Name[0] -ne "_" }

if ($src_files.GetType().Name -eq "Object[]") {
  $num_files = $src_files.Length
} else {
  $num_files = 1
}

Write-Host ("SOURCE FILES (" + $num_files +")") -ForegroundColor blue
$n = 1
foreach ($i in $src_files) {
  echo ($n.ToString() + ": " + $i.FullName)
  $n += 1
}

$compiler = "gcc"

if ($optimization) {
  $flags = "-O2", "-s", "-Wall"
} else {
  $flags = "-O0", "-Wall"
}

$files = ($src_files -join ' ')

$program_include = "-Iinclude"

$nonsense = "-lmingw32", "-lSDl2main", "-lSDL2", "-lSDL2_image", "-lSDL2_ttf"
$nonsense_ = ($nonsense -join " ")

$executable = ("bin\" + $executable_name)

$compile_command = (
  $compiler,
  ($flags -join ' '),
  $files,
  $SDL_include,
  $SDL_library,
  $program_include,
  $nonsense_,
  "-o",
  $executable
) -join ' '

echo ""
Write-Host "COMPILE COMMAND: " -ForegroundColor blue
echo $compile_command
echo ""
Write-Host "COMPILE OUTPUT:" -ForegroundColor blue

&$compiler ($flags) ($src_files) $SDL_include $SDL_library $program_include ($nonsense) -o $executable

echo ""

if ($optimization) {
  Write-Host "STRIPPING EXECUTABLE" -ForegroundColor blue
  strip ($executable + ".exe") -S --strip-unneeded --remove-section=.note.gnu.gold-version --remove-section=.comment --remove-section=.note --remove-section=.note.gnu.build-id --remove-section=.note.ABI-tag
}

Write-Host "COMPLETE" -ForegroundColor green

$host.UI.RawUI.WindowTitle = "Compilation Complete"

cmd /c pause


$host.UI.RawUI.WindowTitle = $old_title
