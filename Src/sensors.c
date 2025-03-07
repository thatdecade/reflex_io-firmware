#include "sensors.h"
#include "stm32f3xx_hal.h"

#define NUM_PADS 4

#define CALIBRATION_ROLLING_WINDOW_SIZE 10

uint8_t sensor_buffer[USB_HID_PACKET_SIZE_BYTES];
uint8_t usb_sensor_buffer[USB_HID_PACKET_SIZE_BYTES];

// Static storage for calibration and state for each pad
static uint16_t pad_idle[NUM_PADS] = {0};            // Calibrated idle values for each pad
static bool     pad_active[NUM_PADS] = {false};      // Current active/inactive flag per pad
static uint32_t pad_last_activation[NUM_PADS] = {0}; // Timestamp of last state change
static uint16_t pad_last_sum[NUM_PADS] = {0};        // Latest sensor sum reading

#define CALIBRATION_DELAY_MS    8000
#define CALIBRATION_DURATION_MS 2000

// SENSOR_THRESHOLD: the amount above idle needed to consider the pad pressed
// SENSOR_HYSTERESIS: to avoid rapid toggling, once active the pad will remain active until sensor reading falls below (THRESHOLD - HYSTERESIS)
// SENSOR_COOLDOWN: minimum time (ms) between state transitions
#define DEFAULT_SENSOR_THRESHOLD   500
#define DEFAULT_SENSOR_HYSTERESIS   50
#define DEFAULT_SENSOR_COOLDOWN    100

// TODO: Replace with Profile Data
#define HARDCODED_SENSOR_THRESHOLD   DEFAULT_SENSOR_THRESHOLD 
#define HARDCODED_SENSOR_HYSTERESIS  DEFAULT_SENSOR_HYSTERESIS
#define HARDCODED_SENSOR_COOLDOWN    DEFAULT_SENSOR_COOLDOWN  

bool sensor_pad_is_active(uint8_t pad)
{
    if (pad >= NUM_PADS) {
        return false;
    }
    return pad_active[pad];
}

void send_request_sensors(void) {
    Request req = request_create(Command_Request_Sensors);
    req.response_len = SENSOR_RESPONSE_LEN;

    req.comport_id = Comport_Left;
    req.response_data = sensor_buffer + ((uint8_t)Comport_Left) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Down;
    req.response_data = sensor_buffer + ((uint8_t)Comport_Down) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Up;
    req.response_data = sensor_buffer + ((uint8_t)Comport_Up) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Right;
    req.response_data = sensor_buffer + ((uint8_t)Comport_Right) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);
}

void process_sensor_data(Response * resp, uint8_t * usb_buffer)
{
    // INDEX: Comport_Left=0, Down=1, Up=2, Right=3
    uint8_t pad_index = (uint8_t)resp->comport_id;
    
    if (pad_index >= NUM_PADS) { // Bound Check
        return;
    }

    if (resp->data_length < SENSOR_RESPONSE_LEN) { // Bound Check
        return;
    }

    // Copy raw sensor data to USB buffer at that pad's offset
    uint8_t offset = pad_index * SENSOR_RESPONSE_LEN;

    // First make a raw copy to send to the host PC
    for (uint8_t i = 0; i < resp->data_length; i++) {
        usb_buffer[offset + i] = resp->data[i];
    }

    // Sum sensor readings
    uint16_t sensor_sum = 0;
    for (uint8_t i = 0; i < SENSOR_RESPONSE_LEN; i += 2) {
        uint16_t reading = resp->data[i] | ((uint16_t)resp->data[i+1] << 8); // Assume little endian
        sensor_sum += reading;
    }
    pad_last_sum[pad_index] = sensor_sum; // Save the sensor sum for calibration purposes.

    // Compute the adjusted reading (subtract idle calibration)
    int32_t adjusted = (int32_t)sensor_sum - (int32_t)pad_idle[pad_index];

    uint32_t current_time = HAL_GetTick();

    // If pad is not active, check if the adjusted value exceeds the threshold
    if (!pad_active[pad_index]) {
        if (adjusted >= HARDCODED_SENSOR_THRESHOLD) {
            // Check that enough time has passed since last activation (cooldown)
            if ((current_time - pad_last_activation[pad_index]) >= HARDCODED_SENSOR_COOLDOWN) {
                pad_active[pad_index] = true;
                pad_last_activation[pad_index] = current_time;
            }
        }
    }
    // If pad is active, check if sensor reading falls below (threshold - hysteresis) to deactivate
    else  if (adjusted < (HARDCODED_SENSOR_THRESHOLD - HARDCODED_SENSOR_HYSTERESIS)) {
        pad_active[pad_index] = false;
        
        // Prevent multiple activations before cooldown
        pad_last_activation[pad_index] = current_time;
    }
}

bool auto_calibrate_sensors(void)
{
    static bool auto_calibration_done = false;
    
    static uint32_t rolling_sum[NUM_PADS] = {0}; // Sum over the window (never grows beyond 32*max(sample))
    static uint16_t rolling_buffer[NUM_PADS][CALIBRATION_ROLLING_WINDOW_SIZE] = {{0}}; // Circular buffer for each pad.
    static uint8_t  rolling_index[NUM_PADS] = {0}; // Next insertion index per pad.
    static uint8_t  rolling_count[NUM_PADS] = {0}; // Number of samples in buffer (max = CALIBRATION_ROLLING_WINDOW_SIZE)

    uint32_t current_time = HAL_GetTick();

    if (auto_calibration_done) 
        return true; // Calibration already completed, nothing to do.
    if (current_time < CALIBRATION_DELAY_MS)
        return false; // Waiting for sensor values to settle after power up

    // For each pad, update its rolling average with the latest sensor sum.
    for (uint8_t i = 0; i < NUM_PADS; i++) {
        if (rolling_count[i] < CALIBRATION_ROLLING_WINDOW_SIZE) {
            // Buffer not full yet: add new sample.
            rolling_sum[i] += pad_last_sum[i];
            rolling_buffer[i][rolling_index[i]] = pad_last_sum[i];
            rolling_index[i] = (rolling_index[i] + 1) % CALIBRATION_ROLLING_WINDOW_SIZE;
            rolling_count[i]++;
        } else {
            // Buffer full: subtract oldest sample then add new sample.
            rolling_sum[i] -= rolling_buffer[i][rolling_index[i]];
            rolling_buffer[i][rolling_index[i]] = pad_last_sum[i];
            rolling_sum[i] += pad_last_sum[i];
            rolling_index[i] = (rolling_index[i] + 1) % CALIBRATION_ROLLING_WINDOW_SIZE;
        }
	
        // Compute the rolling average and update the idle calibration value.
        pad_idle[i] = (uint16_t)(rolling_sum[i] / rolling_count[i]);
	
        // Reset pad activation (prevent stuck states)
        pad_active[i] = false;
        pad_last_activation[i] = current_time;
    }

    // Once the calibration period (delay + duration) has elapsed, mark calibration done.
    if (current_time >= (CALIBRATION_DELAY_MS + CALIBRATION_DURATION_MS)) {
        auto_calibration_done = true;
	
        // Reset pad activation (prevent stuck states)
        for (uint8_t i = 0; i < NUM_PADS; i++) {
            pad_active[i] = false;
            pad_last_activation[i] = current_time;
        }
    }

    return auto_calibration_done;
}
