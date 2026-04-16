Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Launch the app
$process = Start-Process -FilePath ".\gravity_keyboard_sandbox.exe" -PassThru
Start-Sleep -Seconds 1

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
for ($i=0; $i -lt 5; $i++) {
    $bitmap = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($screen.X, $screen.Y, 0, 0, $bitmap.Size)
    $bitmap.Save("c:\Users\Nandi\.gemini\antigravity\brain\f72a3342-0175-4c75-b271-cef80e778ba0\screens\frame_$i.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
    Start-Sleep -Milliseconds 400
}

# Close the app
Stop-Process -Id $process.Id -Force
