
## **Summary**

### **Auto Calibration**  
- On first boot, the firmware will wait for the sensors to settle then set `idle_value` for each sensor.  
- Detection Values are hardcoded
- Future improvment: PC companion app walks user through a calibration wizard. Allowing manual adjustments before saving the profile to the pad's memory.

### **Fail-Safe and Boot-Time Behavior**  
- The keyboard interface will be disabled for 10 seconds after power on.  
- If LED data is received on the Generic HID, the keyboard interface will be disabled until a power cycle.  

### **Task Scheduling**  
- The main loop will call sensor acquisition, sensor processing, LED updates, and keyboard task functions.  
- Future improvment: FreeRTOS or Protothreads to allow better scheduling / threading.

---
# **High-Level Overview of RE:Flex IO Firmware**

### **Main Loop**

The firmware runs multiple HID interfaces to the PC and communicates with the pad boards via serial. 
It concurrently handles LED, sensor, and keypress functions.  

**Tasks include:**  
  - HID communications (both Generic and Keyboard)  
  - LED updates  
  - Sensor monitoring  
  - Keypress events  

### **Operational Constraints**
- During boot:
   - Wait for pad power up and sensor settling (~8 seconds)
   - Take a idle measurement of each pad using a rolling average over 2 seconds.
- During run:
   - If the pad detects LED data sent to the Generic HID, it will permanently disable keyboard keypresses until power cycled.  

### **Profile Management**
- Monitor for PC control packets that save/read profile data.  
- Each profile contains sensor thresholds, hysteresis, cooldown, and key assignments.  
- Profile data is stored in an Emulated EEPROM.

**Profile Defaults:**

| Parameter       | UP         | DOWN       | LEFT       | RIGHT      |
|----------------|-----------|-----------|-----------|-----------|
| **Key Assignment** | UP_ARROW  | DOWN_ARROW | LEFT_ARROW | RIGHT_ARROW |
| **Threshold**  | 260       | 200       | 120       | 260       |
| **Hysteresis**  | 20        | 20        | 20        | 20        |
| **Cooldown**    | 60ms      | 60ms      | 60ms      | 60ms      |

---

## **Sensor Processing**

- The system consists of **16 sensors** (4 pads × 4 sensors each).  
- Each sensor produces a **2-byte reading**.  
- Data from the 4 sensors on a pad is summed for processing.  
- Total sensor data size: **32 bytes across all pads**.  

### **Reading Sensors and PC Forwarding**
- Raw sensor readings are received via UART from the 4 pad boards using `send_request_sensors()`.  
- Responses are processed in `process_sensor_data(resp)`.  
- Raw sensor data is packed and queued for transmission to the PC via the Generic HID interface.  

### **Detection Processing**

**Detection Criteria:**

- **Idle:** Measured at startup, sets the no weight offset.
- **Threshold:** Minimum sensor activation value per pad.
- **Hysteresis:** Sensor release buffer zone, preventing rapid state toggling.
- **Cooldown:** A delay before the pad can register another activation.

**Detection Logic:**

1. **Calculate the pad's raw sensor sum:**  
   ```c
   for (int sensor = 0; sensor < SENSORS_PER_PAD; sensor++) {
       uint16_t reading = data[sensor * 2] | (data[sensor * 2 + 1] << 8);
       pad_raw_value += reading;
   }
   ```
2. **Adjust the reading based on idle measurement:**  
   ```c
   pad_adjusted_value = pad_raw_value - idle_value;
   ```
3. **Determine pad activation using threshold, hysteresis, and cooldown:**  
   ```c
   if (!state && (pad_adjusted_value >= threshold)) {
       if (time_since_last_activation >= cooldown) {
           state = true;
           last_activation_time = current_time;
       }
   } 
   else if (state && (pad_adjusted_value <= (threshold - hysteresis))) {
       state = false;
   }
   //else do nothing
   ```
4. **Save pad active state.**

---

## **Keyboard Processing / Generation**

The keyboard driver is responsible for sending key press reports to the PC via the HID keyboard interface.

- The keyboard task queries each pad's active state over a history.
- When a pad transitions from **inactive -> active**, it triggers a key press.  
- When a pad transitions from **active -> inactive**, it triggers a key release.  

---

## **Lighting**
TBD: The LED control is planned to be **reactive**, animating based on sensor impact.
🚀