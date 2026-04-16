import subprocess
import time
import os

print("Starting the gravity_keyboard_sandbox.exe...")
# Start the executable
process = subprocess.Popen([r".\gravity_keyboard_sandbox.exe"])

# Wait for 1 second to let it initialize
time.sleep(1)

# Now start ffmpeg to capture the screen
print("Starting ffmpeg to capture screen...")
# Capture the main screen for 4 seconds
ffmpeg_process = subprocess.Popen([
    'ffmpeg', '-y', '-f', 'gdigrab', '-framerate', '30', 
    '-i', 'desktop', '-t', '4', 
    r'c:\Users\Nandi\.gemini\antigravity\brain\f72a3342-0175-4c75-b271-cef80e778ba0\exe_recording.mp4'
])

# Wait for ffmpeg to finish
ffmpeg_process.wait()

# Stop the executable
process.kill()
print("Done capturing.")
