Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
  using System;
  using System.Runtime.InteropServices;
  public class Win32 {
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
  }
"@
# Launch the app
$process = Start-Process -FilePath ".\gravity_keyboard_sandbox.exe" -PassThru
Start-Sleep -Seconds 2

# Bring to foreground
[Win32]::SetForegroundWindow($process.MainWindowHandle)
Start-Sleep -Seconds 1

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bitmap = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($screen.X, $screen.Y, 0, 0, $bitmap.Size)
$bitmap.Save("c:\Users\Nandi\.gemini\antigravity\brain\f72a3342-0175-4c75-b271-cef80e778ba0\screens\frame_visible.png", [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()

# Close the app
Stop-Process -Id $process.Id -Force
