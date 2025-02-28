#include "main.h"
#include "stdbool.h"
#include "uart.h"
#include "commands.h"
#include "msgbus.h"
#include "debug_leds.h"
#include "error_handler.h"
#include "commtests.h"
#include "ledtests.h"
#include "color.h"
#include "tusb_config.h"
#include "tusb.h"
#include "tusb_hid.h"
#include "config_mode.h"
#include "profile_config.h"

#define USB_HID_PACKET_SIZE_BYTES (64U)
#define BYTES_PER_SEGMENT (64U)
#define SEGMENTS_PER_PANEL (4U)
#define BYTES_PER_PANEL (BYTES_PER_SEGMENT * SEGMENTS_PER_PANEL)
#define PANELS_PER_PLATFORM (4U)
#define LED_ARRAY_SIZE (BYTES_PER_PANEL * PANELS_PER_PLATFORM)

#define SENSOR_RESPONSE_LEN (8U)

#define COMPLETE_FRAME (0xFFFF)

volatile ErrorCode Panic_Error = 0;
volatile uint32_t Panic_Data = 0;

uint8_t sensor_buffer[USB_HID_PACKET_SIZE_BYTES];
uint8_t usb_sensor_buffer[USB_HID_PACKET_SIZE_BYTES];

volatile uint8_t last_usb_header;
volatile uint32_t packets_fetched = 0;

static void init_system_clock(void);
static void init_gpio(void);

static void init();
static void run();
static void test();
static void process_hid_packet(void);
static inline void process_led_data(uint8_t *packet);

static inline void send_request_sensors() {
    Request req = request_create(Command_Request_Sensors);
    req.response_len = SENSOR_RESPONSE_LEN;

    req.comport_id = Comport_Left;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Left) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Down;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Down) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Up;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Up) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);

    req.comport_id = Comport_Right;
    req.response_data = sensor_buffer + \
        ((uint8_t)Comport_Right) * SENSOR_RESPONSE_LEN;
    msgbus_send_request(req);
}

static inline void send_sensor_update_usb() {
    tud_hid_report(
        USB_SEND_REPORT_ID,
        usb_sensor_buffer,
        USB_HID_PACKET_SIZE_BYTES
    );
}

// Breaks when this is being done after a bunch of times
static inline void process_sensor_data(Response * resp) {
    uint8_t offset = (uint8_t)resp->comport_id * SENSOR_RESPONSE_LEN;

    // Copy data over into usb sensor array
    for (uint8_t i = 0; i < resp->data_length; i++) {
        usb_sensor_buffer[offset + i] = resp->data[i];
    }   
}

static inline void send_commit_LEDs() {
    Request req = request_create(Command_Commit_LEDs);
    req.comport_id = Comport_Left;
    msgbus_send_request(req);
    req.comport_id = Comport_Down;
    msgbus_send_request(req);
    req.comport_id = Comport_Up;
    msgbus_send_request(req);
    req.comport_id = Comport_Right;
    msgbus_send_request(req);
}

static inline void send_process_led_segment(uint8_t panel, uint8_t * data_ptr) {
    Request req = request_create(Command_Process_LED_Segment);
    req.comport_id = (ComportId)panel;
    req.send_data = data_ptr;
    req.send_data_len = BYTES_PER_SEGMENT;
    msgbus_send_request(req);
}


static void process_hid_packet(void) {
    uint8_t *packet = usb_get_packet();
    if (packet == NULL) {
        return;
    }
    
    // First, use the config-mode filter to check for enter/exit packets.
    config_modes_t mode = packet_filter_for_config_mode(packet);
    if (mode != CONFIG_MODE_NORMAL) {
        // Either the "enter" or "exit" magic packet was received.
        set_config_mode(mode);
        return;
    }
    
    // Process LED Data or Config Packets
    if (!is_config_mode()) {
        process_led_data(packet);
    }
    else // is_config_mode
    {
        DBG_LED2_TOGGLE(); // Rapid blink comm LEDs
        
        uint8_t header = packet[0];
        if (header == PROFILE_PUSH_PACKET) {
            // Bytes 1�32 contain sensor thresholds/hysteresis data and bytes 33�36 the panel keys.
            // Save this configuration using the profile_config module.
            HAL_StatusTypeDef status = profile_config_save(packet + 1);
        } else if (header == PROFILE_READ_PACKET) {
            // Prepare a reply packet with header 0xF1 and profile data read from EEPROM.
            uint8_t reply[64] = {0};
            reply[0] = 0xF1;
            profile_config_read(reply + 1);
            tud_hid_report(USB_SEND_REPORT_ID, reply, 64);
        }
    }
}

static inline void process_led_data(uint8_t *packet) {
    static uint16_t segments_received = 0x0000;
    static uint8_t led_buffer[LED_ARRAY_SIZE];
    static uint8_t previous_frame = 0xFF;
    
    // If the previous full frame has been received, commit the LED data.
    if (segments_received == COMPLETE_FRAME) {
        DBG_LED3_ON();
        segments_received = 0x0000;
        send_commit_LEDs();
    }
    
    uint8_t header  = packet[0];
    last_usb_header = header;
    uint8_t panel   = (header >> 6) & 0x03;
    uint8_t segment = (header >> 4) & 0x03;
    uint8_t frame   = header & 0x0F;
    
    uint16_t buffer_offset = panel * BYTES_PER_PANEL + segment * BYTES_PER_SEGMENT;
    
    for (uint8_t i = 0; i < USB_HID_PACKET_SIZE_BYTES; i++) {
        led_buffer[i + buffer_offset] = packet[i];
    }

    if (frame != previous_frame) {
        segments_received = 0x0000;
    }

    previous_frame = frame;
    segments_received |= (1 << (panel * PANELS_PER_PLATFORM + segment));
    send_process_led_segment(panel, led_buffer + buffer_offset);
}

int main(void){
    init();
    //test();
    run();
}

static void init() {
    HAL_Init();
    init_gpio();
    init_system_clock();
    uart_init();
    msgbus_init();
    tusb_init();
    
    DBG_LED1_ON();
}

static void run(void) {
    send_request_sensors();
    
    while (1) {
        // Process any pending messages from the internal (panel-to-panel) comms.
        msgbus_process_flags();
        
        if (msgbus_have_pending_response()) {
            Response *resp = msgbus_get_pending_response();
            switch (resp->request_command) {
                case Command_Request_Sensors:
                    process_sensor_data(resp);
                    break;
                // Add other command responses if needed.
            }
        }
        
        // Instead of directly processing LED data, process any incoming USB HID packets.
        // This will filter out config packets and process profile commands if in config mode.
        process_hid_packet();
        
        // Only send sensor data over USB if we are in normal (non-config) mode.
        if (!is_config_mode()) {
            send_sensor_update_usb();
        }
        
        // Always keep requesting sensor data.
        send_request_sensors();
        
        // Let the TinyUSB stack process USB events.
        tud_task();
    }
}


static void test() {
    // usb comms test
    while (1) {
        tud_task();

        send_sensor_update_usb();
        for (uint8_t i = 0; i < 32; i++) {
            usb_sensor_buffer[i] = i;
        }

        uint8_t * packet = usb_get_packet();

        if (packet == NULL) {
            DBG_LED3_OFF();
            continue;
        }

        uint8_t all_good = true;
        for (uint8_t i = 0; i < 64; i++) {
            if (packet[i] != i) {
                all_good = false;
                break;
            }
        }

        if (all_good) {
            DBG_LED3_ON();
        }
    }

    while(1);

    ComportId port_1 = Comport_Down;
    ComportId port_2 = Comport_Right;

    // Communcation tests for 2 ports
    if (commtest_dual_receive_2bytes(port_1, port_2)
        && commtest_dual_receive_64bytes(port_1, port_2)
        && commtest_dual_double_values(port_1, port_2)) {
        // If they all passed, turn LED 3 on
        DBG_LED3_ON();
    } else {
        DBG_LED3_OFF();
    }

    // test LEDs on panel, hardcoded pattern
    ledtests_hardcoded_LEDs(Comport_Right);
    msgbus_wait_for_idle(Comport_Right);

    // Turn this LED off again to show that we got to this point
    DBG_LED3_OFF();

    HAL_Delay(1000);

    // Ensure comms still work after having dealt with LEDs
    if (commtest_receive_2bytes(Comport_Right)) {
        DBG_LED3_ON();
    } else {
        DBG_LED3_OFF();
    }

    ledtests_loop_color_wheel(Comport_Right);
}

static void init_system_clock() {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        error_panic(Error_HAL_RCC_OscConfig);
    }

    RCC_ClkInitStruct.ClockType = \
        RCC_CLOCKTYPE_HCLK \
        | RCC_CLOCKTYPE_SYSCLK \
        | RCC_CLOCKTYPE_PCLK1 \
        | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        error_panic(Error_HAL_RCC_ClockConfig);
    }

    PeriphClkInit.PeriphClockSelection = \
        RCC_PERIPHCLK_USB \
        | RCC_PERIPHCLK_USART1 \
        | RCC_PERIPHCLK_USART2 \
        | RCC_PERIPHCLK_USART3;
    PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
    PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        error_panic(Error_HAL_RCC_PeriphClockConfig);
    }
}

static void init_gpio() {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // -- Write relevant GPIO pins LOW

    // A 0,1,6,7: Debug pins
    HAL_GPIO_WritePin(
        GPIOA, 
        GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_6 | GPIO_PIN_7,
        GPIO_PIN_RESET
    );

    // C 13,14,15: Status LEDs
    HAL_GPIO_WritePin(
        GPIOC,
        GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
        GPIO_PIN_RESET
    );

    // -- configure pin modes
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // A 0,1,6,7: Debug Pins (outputs)
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // C 13,14,15: Status LEDS (outputs)
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // USB GPIO Configuration    
    // PA11 -> USB_DM
    // PA12 -> USB_DP 
    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_USB;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    __HAL_RCC_USB_CLK_ENABLE();
}

#ifdef  USE_FULL_ASSERT
void assert_failed(char *file, uint32_t line){ }
#endif
