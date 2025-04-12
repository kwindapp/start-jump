Main Topic: Jump Recording with IMU on a Kiteboard Using LoRaWAN & E-Paper Display
Objective:
To record and display jump data (height, airtime, and impact acceleration) on an E-paper display attached to a kiteboard. The data, along with GPS coordinates, will also be sent via LoRaWAN for remote tracking and retrieval.

System Components:
ESP32 (LoRaWAN-enabled) – To handle processing, communication, and connectivity.

IMU (MPU6886 or similar) – To capture acceleration, orientation, and angular velocity for jump data.

GPS Module – To track the real-time location of the kiteboard.

LoRaWAN Module – To transmit data over long distances (e.g., via a LoRa gateway to a cloud server).

E-paper Display – For displaying real-time data on the kiteboard in outdoor conditions.

Battery – For powering the system.

How It Works:
IMU Data for Jump Recording:

The IMU detects changes in acceleration and orientation when the kiteboard is in motion.

During a jump, the IMU records:

Takeoff acceleration: When the board leaves the ground.

Airtime: The time spent in the air (derived from the IMU's acceleration data).

Impact acceleration: When landing, showing the impact force on the board.

Jump height: Calculated based on the airtime and vertical acceleration.

GPS for Real-Time Location:

The GPS module continuously records the latitude and longitude of the kiteboard.

GPS data is used to show the location of the board on the E-paper display.

LoRaWAN for Data Transmission:

Once a jump is detected and the session is complete, the jump data (height, airtime, impact acceleration) and GPS coordinates are sent via LoRaWAN to a remote station or cloud server for storage and tracking.

This data can be retrieved later for analysis or real-time monitoring.

E-paper Display:

The E-paper display shows:

Jump height (cm) – The maximum height reached during the jump.

Airtime (s) – The duration of the jump.

Impact acceleration (g) – The force experienced upon landing.

Location (GPS coordinates) – The current position of the kiteboard.

The display updates dynamically after each jump, providing live performance data while the user is on the water.


![IMG_0553](https://github.com/user-attachments/assets/e02b7047-d51e-402c-a2ab-a63e0c1dafc9)
![IMG_0552](https://github.com/user-attachments/assets/5a632483-6bc4-437e-85e8-c9e301962c63)
