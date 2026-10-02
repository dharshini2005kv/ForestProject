# =============================================
# FOREST SAFETY SYSTEM - Python Controller
# Web Camera + Serial + ThingSpeak IoT
# Run: python forest_system.py
# =============================================

import cv2, serial, requests, time, os
from datetime import datetime

# ========= CHANGE THESE SETTINGS =========
SERIAL_PORT    = 'COM8'           # Windows: COM3, COM4...
                                   # Mac: '/dev/tty.usbmodem...'
                                   # Linux: '/dev/ttyUSB0'
CAMERA_INDEX   = 1                 # 0=first camera, try 1 or 2 if fails
THINGSPEAK_KEY = 'Q0L8ZQ35XHXR5Y1M'   # From thingspeak.com channel
SAVE_FOLDER    = 'animal_photos'  # Where photos are saved
# ==========================================

os.makedirs(SAVE_FOLDER, exist_ok=True)
detection_count = 0
last_distance   = 0.0

# Connect to Arduino
print("Connecting to Arduino...")
try:
    ard = serial.Serial(SERIAL_PORT, 9600, timeout=2)
    time.sleep(2)
    print("Arduino connected!")
except Exception as e:
    print(f"Cannot connect: {e}")
    print("Check SERIAL_PORT setting and that Arduino is plugged in.")
    exit()

# Open web camera
print("Opening camera...")
cam = cv2.VideoCapture(CAMERA_INDEX)
if not cam.isOpened():
    print("Camera failed. Try CAMERA_INDEX = 1 or 2")
    exit()
cam.set(cv2.CAP_PROP_FRAME_WIDTH,  1280)
cam.set(cv2.CAP_PROP_FRAME_HEIGHT,  720)
print("Camera ready!\n")
print("System live. Press Q in camera window to quit.\n")

def capture_photo(dist):
    """Grab frame from webcam, add overlay text, save as JPG"""
    ret, frame = cam.read()
    if not ret:
        print("Camera read failed!")
        return
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fname = f"{SAVE_FOLDER}/animal_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
    # Red alert banner
    cv2.rectangle(frame, (0, 0), (frame.shape[1], 55), (0, 0, 180), -1)
    cv2.putText(frame, "ANIMAL DETECTED!", (15, 35),
                cv2.FONT_HERSHEY_SIMPLEX, 1.1, (255, 255, 255), 2)
    # Info text
    cv2.putText(frame, f"Distance: {dist:.1f} cm   {ts}",
                (15, 75), cv2.FONT_HERSHEY_SIMPLEX,
                0.65, (0, 200, 100), 2)
    cv2.imwrite(fname, frame, [cv2.IMWRITE_JPEG_QUALITY, 95])
    print(f"Photo saved: {fname}")

def upload_thingspeak(dist):
    """Send detection data to ThingSpeak cloud dashboard"""
    try:
        r = requests.get(
            "https://api.thingspeak.com/update",
            params={
                'api_key': THINGSPEAK_KEY,
                'field1': 1,                    # Detection flag
                'field2': round(dist, 1),        # Distance in cm
                'field3': detection_count        # Running count
            },
            timeout=5
        )
        print(f"ThingSpeak updated (entry #{r.text.strip()})")
    except Exception as e:
        print(f"ThingSpeak upload failed: {e}")

# Main loop
try:
    while True:
        # Check for messages from Arduino
        if ard.in_waiting > 0:
            try:
                line = ard.readline().decode('utf-8', errors='ignore').strip()
            except:
                line = ""

            if line.startswith("CAPTURE,"):
                try:
                    last_distance = float(line.split(",")[1])
                except:
                    last_distance = 0.0
                detection_count += 1
                print(f"\nANIMAL DETECTED #{detection_count} — {last_distance:.1f} cm")
                capture_photo(last_distance)
                upload_thingspeak(last_distance)

            elif line.startswith("STATUS,"):
                print(f"Arduino: {line[7:]}")

        # Live camera preview with info overlay
        ret, frame = cam.read()
        if ret:
            cv2.putText(frame,
                f"Detections: {detection_count}   Last: {last_distance:.0f}cm",
                (10, 32), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 80), 2)
            cv2.imshow("Forest Safety System - Live View", frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        time.sleep(0.04)

except KeyboardInterrupt:
    print("\nStopped.")
finally:
    ard.close()
    cam.release()
    cv2.destroyAllWindows()
    print(f"Session ended. Total detections: {detection_count}")
