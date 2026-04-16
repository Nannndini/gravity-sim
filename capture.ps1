Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Launch the app
$process = Start-Process -FilePath ".\gravity_keyboard_sandbox.exe" -PassThru
Start-Sleep -Seconds 5

# Take Screenshot
$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($screen.X, $screen.Y, 0, 0, $bitmap.Size)
$bitmap.Save("c:\Users\Nandi\gravity-sim\screenshot_exe.png", [System.Drawing.Imaging.ImageFormat]::Png)

$graphics.Dispose()
$bitmap.Dispose()

# Close the app
Stop-Process -Id $process.Id -Force
