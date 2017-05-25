/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/** @file
 *
 * @defgroup ble_sdk_app_hids_keyboard_main main.c
 * @{
 * @ingroup ble_sdk_app_hids_keyboard
 * @brief HID Keyboard Sample Application main file.
 *
 * This file contains is the source code for a sample application using the HID, Battery and Device
 * Information Services for implementing a simple keyboard functionality.
 * Pressing Button 0 will send text 'hello' to the connected peer. On receiving output report,
 * it toggles the state of LED 2 on the mother board based on whether or not Caps Lock is on.
 * This application uses the @ref app_scheduler.
 *
 * Also it would accept pairing requests from any peer device.
 */

#include "main.h"
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf_assert.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advertising.h"
#include "ble_advdata.h"
#include "ble_hids.h"
#include "ble_dis.h"
#include "ble_conn_params.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "app_scheduler.h"
#include "softdevice_handler_appsh.h"
#include "app_timer_appsh.h"
#include "device_manager.h"
#include "app_button.h"
#include "pstorage.h"
#include "nrf_delay.h"
#include "app_trace.h"

#include "keyboard_driver.h"
#include "keycode.h"
#include "keymap.h"
#include "battery_service.h"

#ifdef BLE_DFU_APP_SUPPORT
#include "ble_dfu.h"
#include "dfu_app_handler.h"
#endif // BLE_DFU_APP_SUPPORT

#define IS_SRVC_CHANGED_CHARACT_PRESENT 1 /**< Include or not the service_changed characteristic. if not enabled, the server's database cannot be changed for the lifetime of the device*/

#define UART_TX_BUF_SIZE 256 /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1   /**< UART RX buffer size. */

#define DEVICE_NAME "BLE4100"      /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME "Lotlab" /**< Manufacturer. Will be passed to Device Information Service. */

#define PNP_ID_VENDOR_ID_SOURCE 0x02  /**< Vendor ID Source. */
#define PNP_ID_VENDOR_ID 0x1915       /**< Vendor ID. */
#define PNP_ID_PRODUCT_ID 0xEEEE      /**< Product ID. */
#define PNP_ID_PRODUCT_VERSION 0x0001 /**< Product Version. */

#define APP_ADV_FAST_INTERVAL 0x0028 /**< Fast advertising interval (in units of 0.625 ms. This value corresponds to 25 ms.). */
#define APP_ADV_SLOW_INTERVAL 0x0C80 /**< Slow advertising interval (in units of 0.625 ms. This value corrsponds to 2 seconds). */
#define APP_ADV_FAST_TIMEOUT 30      /**< The duration of the fast advertising period (in seconds). */
#define APP_ADV_SLOW_TIMEOUT 180     /**< The duration of the slow advertising period (in seconds). */

#define SLEEP_DEVIDE 10
#define SLEEP_SLOW_TIMEOUT 60 // 60秒后转入慢速扫描模式
#define SLEEP_OFF_TIMEOUT 600 // 10分钟之后自动关机

#define KEYBOARD_SCAN_INTERVAL APP_TIMER_TICKS(25, APP_TIMER_PRESCALER)                  /**< Keyboard scan interval (ticks). */
#define KEYBOARD_SCAN_INTERVAL_SLOW APP_TIMER_TICKS(100, APP_TIMER_PRESCALER)            /**< Keyboard slow scan interval (ticks). */
#define KEYBOARD_FREE_INTERVAL APP_TIMER_TICKS(1000 * SLEEP_DEVIDE, APP_TIMER_PRESCALER) /**< Keyboard sleep interval. */

/*lint -emacro(524, MIN_CONN_INTERVAL) // Loss of precision */
#define MIN_CONN_INTERVAL MSEC_TO_UNITS(12.5, UNIT_1_25_MS) /**< Minimum connection interval (7.5 ms) */
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(60, UNIT_1_25_MS)   /**< Maximum connection interval (30 ms). */
#define SLAVE_LATENCY 6                                     /**< Slave latency. */
#define CONN_SUP_TIMEOUT MSEC_TO_UNITS(850, UNIT_10_MS)     /**< Connection supervisory timeout (430 ms). */

#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER) /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT 3                                            /**< Number of attempts before giving up the connection parameter negotiation. */

#define SEC_PARAM_BOND 1                                        /**< Perform bonding. */
#define SEC_PARAM_MITM 1                                        /**< Man In The Middle protection not required. */
#define SEC_PARAM_IO_CAPABILITIES BLE_GAP_IO_CAPS_KEYBOARD_ONLY /**< No I/O capabilities. */
#define SEC_PARAM_OOB 0                                         /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE 7                                /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE 16                               /**< Maximum encryption key size. */

#define OUTPUT_REPORT_INDEX 0                   /**< Index of Output Report. */
#define OUTPUT_REPORT_MAX_LEN 1                 /**< Maximum length of Output Report. */
#define INPUT_REPORT_KEYS_INDEX 0               /**< Index of Input Report. */
#define OUTPUT_REPORT_BIT_MASK_NUM_LOCK 0x00    /**< NUM LOCK bit in Output Report (based on 'LED Page (0x08)' of the Universal Serial Bus HID Usage Tables). */
#define OUTPUT_REPORT_BIT_MASK_CAPS_LOCK 0x01   /**< CAPS LOCK bit in Output Report (based on 'LED Page (0x08)' of the Universal Serial Bus HID Usage Tables). */
#define OUTPUT_REPORT_BIT_MASK_SCROLL_LOCK 0x02 /**< SCROLL LOCK bit in Output Report (based on 'LED Page (0x08)' of the Universal Serial Bus HID Usage Tables). */
#define INPUT_REP_REF_ID 0                      /**< Id of reference to Keyboard Input Report. */
#define OUTPUT_REP_REF_ID 0                     /**< Id of reference to Keyboard Output Report. */

#define APP_FEATURE_NOT_SUPPORTED BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2 /**< Reply when unsupported features are requested. */

#define MAX_BUFFER_ENTRIES 5 /**< Number of elements that can be enqueued */

#define BASE_USB_HID_SPEC_VERSION 0x0101 /**< Version number of base USB HID Specification implemented by this application. */

#define INPUT_REPORT_KEYS_MAX_LEN 8 /**< Maximum length of the Input Report characteristic. */

#define DEAD_BEEF 0xDEADBEEF /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define SCHED_MAX_EVENT_DATA_SIZE MAX(APP_TIMER_SCHED_EVT_SIZE, \
                                      BLE_STACK_HANDLER_SCHED_EVT_SIZE) /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE 10                                             /**< Maximum number of events in the scheduler queue. */

#define MODIFIER_KEY_POS 0 /**< Position of the modifier byte in the Input Report. */
#define SCAN_CODE_POS 2    /**< This macro indicates the start position of the key scan code in a HID Report. As per the document titled 'Device Class Definition for Human Interface Devices (HID) V1.11, each report shall have one modifier byte followed by a reserved constant byte and then the key scan code. */

#define MAX_KEYS_IN_ONE_REPORT (INPUT_REPORT_KEYS_MAX_LEN - SCAN_CODE_POS) /**< Maximum number of key presses that can be sent in one Input Report. */

#ifdef BLE_DFU_APP_SUPPORT
#define DFU_REV_MAJOR 0x00                                  /** DFU Major revision number to be exposed. */
#define DFU_REV_MINOR 0x00                                  /** DFU Minor revision number to be exposed. */
#define DFU_REVISION ((DFU_REV_MAJOR << 8) | DFU_REV_MINOR) /** DFU Revision number to be exposed. Combined of major and minor versions. */
#define APP_SERVICE_HANDLE_START 0x000C                     /**< Handle of first application specific service when when service changed characteristic is present. */
#define BLE_HANDLE_MAX 0xFFFF                               /**< Max handle value in BLE. */

STATIC_ASSERT(IS_SRVC_CHANGED_CHARACT_PRESENT); /** When having DFU Service support in application the Service Changed Characteristic should always be present. */
#endif                                          // BLE_DFU_APP_SUPPORT

/** 缓存队列访问宏
 *
 * @{ */
/** Initialization of buffer list */
#define BUFFER_LIST_INIT()     \
    do                         \
    {                          \
        buffer_list.rp = 0;    \
        buffer_list.wp = 0;    \
        buffer_list.count = 0; \
    } while (0)

/** Provide status of data list is full or not */
#define BUFFER_LIST_FULL() \
    ((MAX_BUFFER_ENTRIES == buffer_list.count - 1) ? true : false)

/** Provides status of buffer list is empty or not */
#define BUFFER_LIST_EMPTY() \
    ((0 == buffer_list.count) ? true : false)

#define BUFFER_ELEMENT_INIT(i)                 \
    do                                         \
    {                                          \
        buffer_list.buffer[(i)].p_data = NULL; \
    } while (0)

/** @} */

typedef enum {
    BLE_NO_ADV,             /**< No advertising running. */
    BLE_DIRECTED_ADV,       /**< Direct advertising to the latest central. */
    BLE_FAST_ADV_WHITELIST, /**< Advertising with whitelist. */
    BLE_FAST_ADV,           /**< Fast advertising running. */
    BLE_SLOW_ADV,           /**< Slow advertising running. */
    BLE_SLEEP,              /**< Go to system-off. */
} ble_advertising_mode_t;

/** Abstracts buffer element */
typedef struct hid_key_buffer
{
    uint8_t data_offset;    /**< Max Data that can be buffered for all entries */
    uint8_t data_len;       /**< Total length of data */
    uint8_t *p_data;        /**< Scanned key pattern */
    ble_hids_t *p_instance; /**< Identifies peer and service instance */
} buffer_entry_t;

STATIC_ASSERT(sizeof(buffer_entry_t) % 4 == 0);

/** Circular buffer list */
typedef struct
{
    buffer_entry_t buffer[MAX_BUFFER_ENTRIES]; /**< Maximum number of entries that can enqueued in the list */
    uint8_t rp;                                /**< Index to the read location */
    uint8_t wp;                                /**< Index to write location */
    uint8_t count;                             /**< Number of elements in the list */
} buffer_list_t;

STATIC_ASSERT(sizeof(buffer_list_t) % 4 == 0);

static ble_hids_t m_hids;                                /**< Structure used to identify the HID service. */
static bool m_in_boot_mode = false;                      /**< Current protocol mode. */
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID; /**< Handle of the current connection. */
static bool m_led_state[3] = {false};                    /**< LED State. */

APP_TIMER_DEF(m_keyboard_scan_timer_id);
APP_TIMER_DEF(m_keyboard_sleep_timer_id);

static dm_application_instance_t m_app_handle; /**< Application identifier allocated by device manager. */
static dm_handle_t m_bonded_peer_handle;       /**< Device reference handle to the current bonded central. */

#ifdef BLE_DFU_APP_SUPPORT
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE}, {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}, {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE}};
static ble_dfu_t m_dfus; /**< Structure used to identify the DFU service. */
#else
static ble_uuid_t m_adv_uuids[] = {{BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE}, {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE}};
#endif // BLE_DFU_APP_SUPPORT

/** List to enqueue not just data to be sent, but also related information like the handle, connection handle etc */
static buffer_list_t buffer_list;

static void on_hids_evt(ble_hids_t *p_hids, ble_hids_evt_t *p_evt);

static void sleep_mode_enter(void);
static void keys_send(uint8_t key_pattern_len, uint8_t *p_key_pattern);

static uint8_t passkey_enter_index = 0xFF;
static uint8_t passkey_entered[6];
static uint16_t passkey_conn_handle;

static uint16_t sleep_counter = 0;

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in]   line_num   Line number of the failing ASSERT call.
 * @param[in]   file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for handling Service errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void service_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling advertising errors.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void ble_advertising_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void keyboard_scan_timeout_handler(void *p_context);
static void keyboard_sleep_timeout_handler(void *p_context);
/**@brief 计时器初始化函数
 *
 * @details Initializes the timer module.
 */
static void timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module, making it use the scheduler.
    APP_TIMER_APPSH_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, true);

    err_code = app_timer_create(&m_keyboard_scan_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                keyboard_scan_timeout_handler);

    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_keyboard_sleep_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                keyboard_sleep_timeout_handler);

    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    uint32_t err_code;
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_HID_KEYBOARD);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_tx_power_set(-4);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing Device Information Service.
 */
static void dis_init(void)
{
    uint32_t err_code;
    ble_dis_init_t dis_init_obj;
    ble_dis_pnp_id_t pnp_id;

    pnp_id.vendor_id_source = PNP_ID_VENDOR_ID_SOURCE;
    pnp_id.vendor_id = PNP_ID_VENDOR_ID;
    pnp_id.product_id = PNP_ID_PRODUCT_ID;
    pnp_id.product_version = PNP_ID_PRODUCT_VERSION;

    memset(&dis_init_obj, 0, sizeof(dis_init_obj));

    ble_srv_ascii_to_utf8(&dis_init_obj.manufact_name_str, MANUFACTURER_NAME);
    dis_init_obj.p_pnp_id = &pnp_id;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&dis_init_obj.dis_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init_obj.dis_attr_md.write_perm);

    err_code = ble_dis_init(&dis_init_obj);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing HID Service.
 */
static void hids_init(void)
{
    uint32_t err_code;
    ble_hids_init_t hids_init_obj;
    ble_hids_inp_rep_init_t input_report_array[1];
    ble_hids_inp_rep_init_t *p_input_report;
    ble_hids_outp_rep_init_t output_report_array[1];
    ble_hids_outp_rep_init_t *p_output_report;
    uint8_t hid_info_flags;

    memset((void *)input_report_array, 0, sizeof(ble_hids_inp_rep_init_t));
    memset((void *)output_report_array, 0, sizeof(ble_hids_outp_rep_init_t));

    static uint8_t report_map_data[] =
        {
            0x05, 0x01, // Usage Page (Generic Desktop)
            0x09, 0x06, // Usage (Keyboard)
            0xA1, 0x01, // Collection (Application)
            0x05, 0x07, //     Usage Page (Key Codes)
            0x19, 0xe0, //     Usage Minimum (224)
            0x29, 0xe7, //     Usage Maximum (231)
            0x15, 0x00, //     Logical Minimum (0)
            0x25, 0x01, //     Logical Maximum (1)
            0x75, 0x01, //     Report Size (1)
            0x95, 0x08, //     Report Count (8)
            0x81, 0x02, //     Input (Data, Variable, Absolute)

            0x95, 0x01, //     Report Count (1)
            0x75, 0x08, //     Report Size (8)
            0x81, 0x01, //     Input (Constant) reserved byte(1)

            0x95, 0x05, //     Report Count (5)
            0x75, 0x01, //     Report Size (1)
            0x05, 0x08, //     Usage Page (Page# for LEDs)
            0x19, 0x01, //     Usage Minimum (1)
            0x29, 0x05, //     Usage Maximum (5)
            0x91, 0x02, //     Output (Data, Variable, Absolute), Led report
            0x95, 0x01, //     Report Count (1)
            0x75, 0x03, //     Report Size (3)
            0x91, 0x01, //     Output (Data, Variable, Absolute), Led report padding

            0x95, 0x06, //     Report Count (6)
            0x75, 0x08, //     Report Size (8)
            0x15, 0x00, //     Logical Minimum (0)
            0x25, 0x65, //     Logical Maximum (255)
            0x05, 0x07, //     Usage Page (Key codes)
            0x19, 0x00, //     Usage Minimum (0)
            0x29, 0x65, //     Usage Maximum (101)
            0x81, 0x00, //     Input (Data, Array) Key array(6 bytes)

            0x09, 0x05,       //     Usage (Vendor Defined)
            0x15, 0x00,       //     Logical Minimum (0)
            0x26, 0xFF, 0x00, //     Logical Maximum (255)
            0x75, 0x08,       //     Report Count (2)
            0x95, 0x02,       //     Report Size (8 bit)
            0xB1, 0x02,       //     Feature (Data, Variable, Absolute)

            0xC0 // End Collection (Application)
        };

    // Initialize HID Service
    p_input_report = &input_report_array[INPUT_REPORT_KEYS_INDEX];
    p_input_report->max_len = INPUT_REPORT_KEYS_MAX_LEN;
    p_input_report->rep_ref.report_id = INPUT_REP_REF_ID;
    p_input_report->rep_ref.report_type = BLE_HIDS_REP_TYPE_INPUT;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_input_report->security_mode.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_input_report->security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_input_report->security_mode.write_perm);

    p_output_report = &output_report_array[OUTPUT_REPORT_INDEX];
    p_output_report->max_len = OUTPUT_REPORT_MAX_LEN;
    p_output_report->rep_ref.report_id = OUTPUT_REP_REF_ID;
    p_output_report->rep_ref.report_type = BLE_HIDS_REP_TYPE_OUTPUT;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_output_report->security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&p_output_report->security_mode.write_perm);

    hid_info_flags = HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;

    memset(&hids_init_obj, 0, sizeof(hids_init_obj));

    hids_init_obj.evt_handler = on_hids_evt;
    hids_init_obj.error_handler = service_error_handler;
    hids_init_obj.is_kb = true;
    hids_init_obj.is_mouse = false;
    hids_init_obj.inp_rep_count = 1;
    hids_init_obj.p_inp_rep_array = input_report_array;
    hids_init_obj.outp_rep_count = 1;
    hids_init_obj.p_outp_rep_array = output_report_array;
    hids_init_obj.feature_rep_count = 0;
    hids_init_obj.p_feature_rep_array = NULL;
    hids_init_obj.rep_map.data_len = sizeof(report_map_data);
    hids_init_obj.rep_map.p_data = report_map_data;
    hids_init_obj.hid_information.bcd_hid = BASE_USB_HID_SPEC_VERSION;
    hids_init_obj.hid_information.b_country_code = 0;
    hids_init_obj.hid_information.flags = hid_info_flags;
    hids_init_obj.included_services_count = 0;
    hids_init_obj.p_included_services_array = NULL;

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.rep_map.security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.rep_map.security_mode.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.hid_information.security_mode.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.hid_information.security_mode.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(
        &hids_init_obj.security_mode_boot_kb_inp_rep.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_boot_kb_inp_rep.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.security_mode_boot_kb_inp_rep.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_boot_kb_outp_rep.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_boot_kb_outp_rep.write_perm);

    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_protocol.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_protocol.write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&hids_init_obj.security_mode_ctrl_point.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&hids_init_obj.security_mode_ctrl_point.write_perm);

    err_code = ble_hids_init(&m_hids, &hids_init_obj);
    APP_ERROR_CHECK(err_code);
}

/** DFU 相关 **/
#ifdef BLE_DFU_APP_SUPPORT
/**@brief 停止广播
 */
static void advertising_stop(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_stop();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for loading application-specific context after establishing a secure connection.
 *
 * @details This function will load the application context and check if the ATT table is marked as 
 *          changed. If the ATT table is marked as changed, a Service Changed Indication
 *          is sent to the peer if the Service Changed CCCD is set to indicate.
 *
 * @param[in] p_handle The Device Manager handle that identifies the connection for which the context 
 *                     should be loaded.
 */
static void app_context_load(dm_handle_t const *p_handle)
{
    uint32_t err_code;
    static uint32_t context_data;
    dm_application_context_t context;

    context.len = sizeof(context_data);
    context.p_data = (uint8_t *)&context_data;

    err_code = dm_application_context_get(p_handle, &context);
    if (err_code == NRF_SUCCESS)
    {
        // Send Service Changed Indication if ATT table has changed.
        if ((context_data & (DFU_APP_ATT_TABLE_CHANGED << DFU_APP_ATT_TABLE_POS)) != 0)
        {
            err_code = sd_ble_gatts_service_changed(m_conn_handle, APP_SERVICE_HANDLE_START, BLE_HANDLE_MAX);
            if ((err_code != NRF_SUCCESS) &&
                (err_code != BLE_ERROR_INVALID_CONN_HANDLE) &&
                (err_code != NRF_ERROR_INVALID_STATE) &&
                (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
                (err_code != NRF_ERROR_BUSY) &&
                (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
            {
                APP_ERROR_HANDLER(err_code);
            }
        }

        err_code = dm_application_context_delete(p_handle);
        APP_ERROR_CHECK(err_code);
    }
    else if (err_code == DM_NO_APP_CONTEXT)
    {
        // No context available. Ignore.
    }
    else
    {
        APP_ERROR_HANDLER(err_code);
    }
}

/** @snippet [DFU BLE Reset prepare] */
/**@brief Function for preparing for system reset.
 *
 * @details This function implements @ref dfu_app_reset_prepare_t. It will be called by 
 *          @ref dfu_app_handler.c before entering the bootloader/DFU.
 *          This allows the current running application to shut down gracefully.
 */
static void reset_prepare(void)
{
    uint32_t err_code;

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        // Disconnect from peer.
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
    }
    else
    {
        // If not connected, the device will be advertising. Hence stop the advertising.
        advertising_stop();
    }

    err_code = ble_conn_params_stop();
    APP_ERROR_CHECK(err_code);

    nrf_delay_ms(500);
}

static void dfu_init(void)
{
    uint32_t err_code;
    ble_dfu_init_t dfus_init;

    // Initialize the Device Firmware Update Service.
    memset(&dfus_init, 0, sizeof(dfus_init));

    dfus_init.evt_handler = dfu_app_on_dfu_evt;
    dfus_init.error_handler = NULL;
    dfus_init.evt_handler = dfu_app_on_dfu_evt;
    dfus_init.revision = DFU_REVISION;

    err_code = ble_dfu_init(&m_dfus, &dfus_init);
    APP_ERROR_CHECK(err_code);

    dfu_app_reset_prepare_set(reset_prepare);
    dfu_app_dm_appl_instance_set(m_app_handle);
}
/** @snippet [DFU BLE Reset prepare] */
#endif // BLE_DFU_APP_SUPPORT

/**@brief 初始化程序所需的服务
 */
static void services_init(void)
{
    dis_init();
    hids_init();
#ifdef BLE_DFU_APP_SUPPORT
    dfu_init();
#endif // BLE_DFU_APP_SUPPORT
}

static bool keyboard_conn_pass_enter_handler(const uint8_t *key_packet, uint8_t key_packet_size)
{
    if (passkey_enter_index < 6)
    {
        for (uint_fast8_t i = 0; i < key_packet_size; i++)
        {
            if (key_packet[i] >= KC_1)
            {
                if (key_packet[i] <= KC_0)
                {
                    passkey_entered[passkey_enter_index++] = (key_packet[i] + 1 - KC_1) % 10 + '0';
                }
                else if (key_packet[i] >= KC_KP_1 && key_packet[i] <= KC_KP_0)
                {
                    passkey_entered[passkey_enter_index++] = (key_packet[i] + 1 - KC_KP_1) % 10 + '0';
                }
            }
        }
        if (passkey_enter_index == 6)
        {
            sd_ble_gap_auth_key_reply(passkey_conn_handle, BLE_GAP_AUTH_KEY_TYPE_PASSKEY, passkey_entered);
        }
        return true;
    }
    else
    {
        return false;
    }
}

static void keyboard_switch_scan_mode(bool slow)
{
    uint32_t err_code;
    err_code = app_timer_stop(m_keyboard_scan_timer_id);
    if (slow)
        err_code = app_timer_start(m_keyboard_scan_timer_id, KEYBOARD_SCAN_INTERVAL_SLOW, NULL);
    else
        err_code = app_timer_start(m_keyboard_scan_timer_id, KEYBOARD_SCAN_INTERVAL, NULL);

    APP_ERROR_CHECK(err_code);
}

static void keyboard_sleep_timeout_handler(void *p_context)
{
    sleep_counter++;
    if (sleep_counter == SLEEP_SLOW_TIMEOUT / SLEEP_DEVIDE)
    {
        keyboard_switch_scan_mode(true);
    }
    else if (sleep_counter == SLEEP_OFF_TIMEOUT / SLEEP_DEVIDE)
    {
        sleep_mode_enter();
    }
}

static void keyboard_sleep_counter_reset(void)
{
    if (sleep_counter >= SLEEP_SLOW_TIMEOUT / SLEEP_DEVIDE)
    {
        keyboard_switch_scan_mode(false);
    }

    sleep_counter = 0;
}

/**@brief Function for handling the keyboard scan timer timeout.
 *
 * @details This function will be called each time the keyboard scan timer expires.
 *
 */
static void keyboard_scan_timeout_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);
    const uint8_t *key_packet;
    uint8_t key_packet_size;
    if (new_packet(&key_packet, &key_packet_size))
    {
        keyboard_sleep_counter_reset();
        for (uint_fast8_t i = 0; i < key_packet_size; i++)
        {
            if (key_packet[i] == KC_FN15)
            {
                sleep_mode_enter();
            }
        }
        if (keyboard_conn_pass_enter_handler(key_packet, key_packet_size))
            return; // 处理配对密码输入事件
        if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
        {
            keys_send(key_packet_size, (uint8_t *)&key_packet[0]);
        }
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail = false;
    cp_init.evt_handler = NULL;
    cp_init.error_handler = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief 计时器启动函数
 */
static void timers_start(void)
{
    uint32_t err_code;

    err_code = app_timer_start(m_keyboard_scan_timer_id, KEYBOARD_SCAN_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_start(m_keyboard_sleep_timer_id, KEYBOARD_FREE_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);

    battery_timer_start();
}

/**@brief   Function for transmitting a key scan Press & Release Notification.
 *
 * @warning This handler is an example only. You need to analyze how you wish to send the key
 *          release.
 *
 * @param[in]  p_instance     Identifies the service for which Key Notifications are requested.
 * @param[in]  p_key_pattern  Pointer to key pattern.
 * @param[in]  pattern_len    Length of key pattern. 0 < pattern_len < 7.
 * @param[in]  pattern_offset Offset applied to Key Pattern for transmission.
 * @param[out] actual_len     Provides actual length of Key Pattern transmitted, making buffering of
 *                            rest possible if needed.
 * @return     NRF_SUCCESS on success, BLE_ERROR_NO_TX_BUFFERS in case transmission could not be
 *             completed due to lack of transmission buffer or other error codes indicating reason
 *             for failure.
 *
 */
static uint32_t send_key_scan_press_release(ble_hids_t *p_hids,
                                            uint8_t *p_key_pattern,
                                            uint16_t pattern_len,
                                            uint16_t pattern_offset,
                                            uint16_t *p_actual_len)
{
    uint32_t err_code;
    for (int i = 0; i < pattern_len; i++)
    {
        switch (p_key_pattern[i])
        {
        case KC_NUMLOCK:
            m_led_state[OUTPUT_REPORT_BIT_MASK_NUM_LOCK] = !m_led_state[OUTPUT_REPORT_BIT_MASK_NUM_LOCK];
            nrf_gpio_pin_toggle(LED_NUM);
            break;
        case KC_CAPSLOCK:
            m_led_state[OUTPUT_REPORT_BIT_MASK_CAPS_LOCK] = !m_led_state[OUTPUT_REPORT_BIT_MASK_CAPS_LOCK];
            nrf_gpio_pin_toggle(LED_CAPS);
            break;
        case KC_SCROLLLOCK:
            m_led_state[OUTPUT_REPORT_BIT_MASK_SCROLL_LOCK] = !m_led_state[OUTPUT_REPORT_BIT_MASK_SCROLL_LOCK];
            nrf_gpio_pin_toggle(LED_SCLK);
            break;
        default:
            break;
        }
    }
    if (!m_in_boot_mode)
    {
        err_code = ble_hids_inp_rep_send(&m_hids,
                                         INPUT_REPORT_KEYS_INDEX,
                                         INPUT_REPORT_KEYS_MAX_LEN,
                                         p_key_pattern);
    }
    else
    {
        err_code = ble_hids_boot_kb_inp_rep_send(&m_hids,
                                                 INPUT_REPORT_KEYS_MAX_LEN,
                                                 p_key_pattern);
    }

    return err_code;
}

/**@brief   Function for initializing the buffer queue used to key events that could not be
 *          transmitted
 *
 * @warning This handler is an example only. You need to analyze how you wish to buffer or buffer at
 *          all.
 *
 * @note    In case of HID keyboard, a temporary buffering could be employed to handle scenarios
 *          where encryption is not yet enabled or there was a momentary link loss or there were no
 *          Transmit buffers.
 */
static void buffer_init(void)
{
    uint32_t buffer_count;

    BUFFER_LIST_INIT();

    for (buffer_count = 0; buffer_count < MAX_BUFFER_ENTRIES; buffer_count++)
    {
        BUFFER_ELEMENT_INIT(buffer_count);
    }
}

/**@brief Function for enqueuing key scan patterns that could not be transmitted either completely
 *        or partially.
 *
 * @warning This handler is an example only. You need to analyze how you wish to send the key
 *          release.
 *
 * @param[in]  p_hids         Identifies the service for which Key Notifications are buffered.
 * @param[in]  p_key_pattern  Pointer to key pattern.
 * @param[in]  pattern_len    Length of key pattern.
 * @param[in]  offset         Offset applied to Key Pattern when requesting a transmission on
 *                            dequeue, @ref buffer_dequeue.
 * @return     NRF_SUCCESS on success, else an error code indicating reason for failure.
 */
static uint32_t buffer_enqueue(ble_hids_t *p_hids,
                               uint8_t *p_key_pattern,
                               uint16_t pattern_len,
                               uint16_t offset)
{
    buffer_entry_t *element;
    uint32_t err_code = NRF_SUCCESS;

    if (BUFFER_LIST_FULL())
    {
        // Element cannot be buffered.
        err_code = NRF_ERROR_NO_MEM;
    }
    else
    {
        // Make entry of buffer element and copy data.
        element = &buffer_list.buffer[(buffer_list.wp)];
        element->p_instance = p_hids;
        element->p_data = p_key_pattern;
        element->data_offset = offset;
        element->data_len = pattern_len;

        buffer_list.count++;
        buffer_list.wp++;

        if (buffer_list.wp == MAX_BUFFER_ENTRIES)
        {
            buffer_list.wp = 0;
        }
    }

    return err_code;
}

/**@brief   Function to dequeue key scan patterns that could not be transmitted either completely of
 *          partially.
 *
 * @warning This handler is an example only. You need to analyze how you wish to send the key
 *          release.
 *
 * @param[in]  tx_flag   Indicative of whether the dequeue should result in transmission or not.
 * @note       A typical example when all keys are dequeued with transmission is when link is
 *             disconnected.
 *
 * @return     NRF_SUCCESS on success, else an error code indicating reason for failure.
 */
static uint32_t buffer_dequeue(bool tx_flag)
{
    buffer_entry_t *p_element;
    uint32_t err_code = NRF_SUCCESS;
    uint16_t actual_len;

    if (BUFFER_LIST_EMPTY())
    {
        err_code = NRF_ERROR_NOT_FOUND;
    }
    else
    {
        bool remove_element = true;

        p_element = &buffer_list.buffer[(buffer_list.rp)];

        if (tx_flag)
        {
            err_code = send_key_scan_press_release(p_element->p_instance,
                                                   p_element->p_data,
                                                   p_element->data_len,
                                                   p_element->data_offset,
                                                   &actual_len);
            // An additional notification is needed for release of all keys, therefore check
            // is for actual_len <= element->data_len and not actual_len < element->data_len
            if ((err_code == BLE_ERROR_NO_TX_BUFFERS) && (actual_len <= p_element->data_len))
            {
                // Transmission could not be completed, do not remove the entry, adjust next data to
                // be transmitted
                p_element->data_offset = actual_len;
                remove_element = false;
            }
        }

        if (remove_element)
        {
            BUFFER_ELEMENT_INIT(buffer_list.rp);

            buffer_list.rp++;
            buffer_list.count--;

            if (buffer_list.rp == MAX_BUFFER_ENTRIES)
            {
                buffer_list.rp = 0;
            }
        }
    }

    return err_code;
}

/**@brief Function for sending sample key presses to the peer.
 *
 * @param[in]   key_pattern_len   Pattern length.
 * @param[in]   p_key_pattern     Pattern to be sent.
 */
static void keys_send(uint8_t key_pattern_len, uint8_t *p_key_pattern)
{
    uint32_t err_code;
    uint16_t actual_len;

    err_code = send_key_scan_press_release(&m_hids,
                                           p_key_pattern,
                                           key_pattern_len,
                                           0,
                                           &actual_len);
    // An additional notification is needed for release of all keys, therefore check
    // is for actual_len <= key_pattern_len and not actual_len < key_pattern_len.
    if ((err_code == BLE_ERROR_NO_TX_BUFFERS) && (actual_len <= key_pattern_len))
    {
        // Buffer enqueue routine return value is not intentionally checked.
        // Rationale: Its better to have a a few keys missing than have a system
        // reset. Recommendation is to work out most optimal value for
        // MAX_BUFFER_ENTRIES to minimize chances of buffer queue full condition
        UNUSED_VARIABLE(buffer_enqueue(&m_hids, p_key_pattern, key_pattern_len, actual_len));
    }

    if ((err_code != NRF_SUCCESS) &&
        (err_code != NRF_ERROR_INVALID_STATE) &&
        (err_code != BLE_ERROR_NO_TX_BUFFERS) &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING))
    {
        APP_ERROR_HANDLER(err_code);
    }
}

/**@brief Function for handling the HID Report Characteristic Write event.
 *
 * @param[in]   p_evt   HID service event.
 */
static void on_hid_rep_char_write(ble_hids_evt_t *p_evt)
{
    if (p_evt->params.char_write.char_id.rep_type == BLE_HIDS_REP_TYPE_OUTPUT)
    {
        uint32_t err_code;
        uint8_t report_val;
        uint8_t report_index = p_evt->params.char_write.char_id.rep_index;

        if (report_index == OUTPUT_REPORT_INDEX)
        {
            // This code assumes that the outptu report is one byte long. Hence the following
            // static assert is made.
            STATIC_ASSERT(OUTPUT_REPORT_MAX_LEN == 1);

            err_code = ble_hids_outp_rep_get(&m_hids,
                                             report_index,
                                             OUTPUT_REPORT_MAX_LEN,
                                             0,
                                             &report_val);
            APP_ERROR_CHECK(err_code);

            if (report_val & 1 << OUTPUT_REPORT_BIT_MASK_NUM_LOCK)
            {
                nrf_gpio_pin_set(LED_NUM);
                m_led_state[OUTPUT_REPORT_BIT_MASK_NUM_LOCK] = true;
            }
            else
            {
                nrf_gpio_pin_clear(LED_NUM);
                m_led_state[OUTPUT_REPORT_BIT_MASK_NUM_LOCK] = false;
            }

            if (report_val & 1 << OUTPUT_REPORT_BIT_MASK_CAPS_LOCK)
            {
                nrf_gpio_pin_set(LED_CAPS);
                m_led_state[OUTPUT_REPORT_BIT_MASK_CAPS_LOCK] = true;
            }
            else
            {
                nrf_gpio_pin_clear(LED_CAPS);
                m_led_state[OUTPUT_REPORT_BIT_MASK_CAPS_LOCK] = false;
            }

            if (report_val & 1 << OUTPUT_REPORT_BIT_MASK_SCROLL_LOCK)
            {
                nrf_gpio_pin_set(LED_SCLK);
                m_led_state[OUTPUT_REPORT_BIT_MASK_SCROLL_LOCK] = true;
            }
            else
            {
                nrf_gpio_pin_clear(LED_SCLK);
                m_led_state[OUTPUT_REPORT_BIT_MASK_SCROLL_LOCK] = false;
            }

            // The report received is not supported by this application. Do nothing.
        }
    }
}

static void led_notice(void)
{
    nrf_gpio_pin_set(LED_NUM);
    nrf_gpio_pin_set(LED_CAPS);
    nrf_gpio_pin_set(LED_SCLK);
    nrf_delay_ms(100);
    nrf_gpio_pin_clear(LED_NUM);
    nrf_gpio_pin_clear(LED_CAPS);
    nrf_gpio_pin_clear(LED_SCLK);
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code;

    sleep_mode_prepare();
    led_notice();

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling HID events.
 *
 * @details This function will be called for all HID events which are passed to the application.
 *
 * @param[in]   p_hids  HID service structure.
 * @param[in]   p_evt   Event received from the HID service.
 */
static void on_hids_evt(ble_hids_t *p_hids, ble_hids_evt_t *p_evt)
{
    switch (p_evt->evt_type)
    {
    case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
        m_in_boot_mode = true;
        break;

    case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
        m_in_boot_mode = false;
        break;

    case BLE_HIDS_EVT_REP_CHAR_WRITE:
        on_hid_rep_char_write(p_evt);
        break;

    case BLE_HIDS_EVT_NOTIF_ENABLED:
    {
        dm_service_context_t service_context;
        service_context.service_type = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;
        service_context.context_data.len = 0;
        service_context.context_data.p_data = NULL;

        if (m_in_boot_mode)
        {
            // Protocol mode is Boot Protocol mode.
            if (
                p_evt->params.notification.char_id.uuid ==
                BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR)
            {
                // The notification of boot keyboard input report has been enabled.
                // Save the system attribute (CCCD) information into the flash.
                uint32_t err_code;

                err_code = dm_service_context_set(&m_bonded_peer_handle, &service_context);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
                else
                {
                    // The system attributes could not be written to the flash because
                    // the connected central is not a new central. The system attributes
                    // will only be written to flash only when disconnected from this central.
                    // Do nothing now.
                }
            }
            else
            {
                // Do nothing.
            }
        }
        else if (p_evt->params.notification.char_id.rep_type == BLE_HIDS_REP_TYPE_INPUT)
        {
            // The protocol mode is Report Protocol mode. And the CCCD for the input report
            // is changed. It is now time to store all the CCCD information (system
            // attributes) into the flash.
            uint32_t err_code;

            err_code = dm_service_context_set(&m_bonded_peer_handle, &service_context);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            else
            {
                // The system attributes could not be written to the flash because
                // the connected central is not a new central. The system attributes
                // will only be written to flash only when disconnected from this central.
                // Do nothing now.
            }
        }
        else
        {
            // The notification of the report that was enabled by the central is not interesting
            // to this application. So do nothing.
        }
        break;
    }

    default:
        // No implementation needed.
        break;
    }
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
    case BLE_ADV_EVT_DIRECTED:
        //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_DIRECTED);
        //APP_ERROR_CHECK(err_code);
        break;
    case BLE_ADV_EVT_FAST:
        //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
        //APP_ERROR_CHECK(err_code);
        break;
    case BLE_ADV_EVT_SLOW:
        //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_SLOW);
        //APP_ERROR_CHECK(err_code);
        break;
    case BLE_ADV_EVT_FAST_WHITELIST:
        //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_WHITELIST);
        //APP_ERROR_CHECK(err_code);
        break;
    case BLE_ADV_EVT_SLOW_WHITELIST:
        //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING_WHITELIST);
        //APP_ERROR_CHECK(err_code);
        break;
    case BLE_ADV_EVT_IDLE:
        sleep_mode_enter();
        break;

    case BLE_ADV_EVT_WHITELIST_REQUEST:
    {
        ble_gap_whitelist_t whitelist;
        ble_gap_addr_t *p_whitelist_addr[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
        ble_gap_irk_t *p_whitelist_irk[BLE_GAP_WHITELIST_IRK_MAX_COUNT];

        whitelist.addr_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
        whitelist.irk_count = BLE_GAP_WHITELIST_IRK_MAX_COUNT;
        whitelist.pp_addrs = p_whitelist_addr;
        whitelist.pp_irks = p_whitelist_irk;

        err_code = dm_whitelist_create(&m_app_handle, &whitelist);
        APP_ERROR_CHECK(err_code);

        err_code = ble_advertising_whitelist_reply(&whitelist);
        APP_ERROR_CHECK(err_code);
        break;
    }
    case BLE_ADV_EVT_PEER_ADDR_REQUEST:
    {
        ble_gap_addr_t peer_address;

        // Only Give peer address if we have a handle to the bonded peer.
        if (m_bonded_peer_handle.appl_id != DM_INVALID_ID)
        {

            err_code = dm_peer_addr_get(&m_bonded_peer_handle, &peer_address);
            APP_ERROR_CHECK(err_code);

            err_code = ble_advertising_peer_addr_reply(&peer_address);
            APP_ERROR_CHECK(err_code);
        }
        break;
    }
    default:
        break;
    }
}

/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t *p_ble_evt)
{
    uint32_t err_code;
    ble_gatts_rw_authorize_reply_params_t auth_reply;

    switch (p_ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
        m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        break;

    case BLE_EVT_TX_COMPLETE:
        // Send next key event
        (void)buffer_dequeue(true);
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        // Dequeue all keys without transmission.
        (void)buffer_dequeue(false);

        m_conn_handle = BLE_CONN_HANDLE_INVALID;

        // Reset m_caps_on variable. Upon reconnect, the HID host will re-send the Output
        // report containing the Caps lock state.
        break;

    case BLE_EVT_USER_MEM_REQUEST:
        err_code = sd_ble_user_mem_reply(m_conn_handle, NULL);
        APP_ERROR_CHECK(err_code);
        break;

    case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
        if (p_ble_evt->evt.gatts_evt.params.authorize_request.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
        {
            if ((p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ) || (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) || (p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
            {
                if (p_ble_evt->evt.gatts_evt.params.authorize_request.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                {
                    auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                }
                else
                {
                    auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                }
                auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                err_code = sd_ble_gatts_rw_authorize_reply(m_conn_handle, &auth_reply);
                APP_ERROR_CHECK(err_code);
            }
        }
        break;

    case BLE_GAP_EVT_AUTH_KEY_REQUEST:
        passkey_enter_index = 0;
        passkey_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
        break;

    case BLE_GATTC_EVT_TIMEOUT:
    case BLE_GATTS_EVT_TIMEOUT:
        // Disconnect on GATT Server and Client timeout events.
        err_code = sd_ble_gap_disconnect(m_conn_handle,
                                         BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        break;

    default:
        // No implementation needed.
        break;
    }
    //nrf_gpio_pin_toggle(LED_NUM);
}

/**@brief   Function for dispatching a BLE stack event to all modules with a BLE stack event handler.
 *
 * @details This function is called from the scheduler in the main loop after a BLE stack
 *          event has been received.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 */
static void ble_evt_dispatch(ble_evt_t *p_ble_evt)
{
    dm_ble_evt_handler(p_ble_evt);
#ifdef BLE_DFU_APP_SUPPORT /** @snippet [Propagating BLE Stack events to DFU Service] */
    ble_dfu_on_ble_evt(&m_dfus, p_ble_evt);
#endif // BLE_DFU_APP_SUPPORT
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_hids_on_ble_evt(&m_hids, p_ble_evt);
    battery_service_ble_evt(p_ble_evt);
}

/**@brief   Function for dispatching a system event to interested modules.
 *
 * @details This function is called from the System event interrupt handler after a system
 *          event has been received.
 *
 * @param[in]   sys_evt   System stack event.
 */
static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    ble_advertising_on_sys_evt(sys_evt);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_APPSH_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION, true);

    // Enable BLE stack
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
#if (defined(S130) || defined(S132))
    ble_enable_params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT;
#endif
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the Event Scheduler initialization.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t err_code;
    uint8_t adv_flags;
    ble_advdata_t advdata;

    // Build and set advertising data
    memset(&advdata, 0, sizeof(advdata));

    adv_flags = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags = adv_flags;
    advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    advdata.uuids_complete.p_uuids = m_adv_uuids;

    ble_adv_modes_config_t options =
        {
            BLE_ADV_WHITELIST_ENABLED,
            BLE_ADV_DIRECTED_ENABLED,
            BLE_ADV_DIRECTED_SLOW_DISABLED, 0, 0,
            BLE_ADV_FAST_ENABLED, APP_ADV_FAST_INTERVAL, APP_ADV_FAST_TIMEOUT,
            BLE_ADV_SLOW_ENABLED, APP_ADV_SLOW_INTERVAL, APP_ADV_SLOW_TIMEOUT};

    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, ble_advertising_error_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Device Manager events.
 *
 * @param[in]   p_evt   Data associated to the device manager event.
 */
static uint32_t device_manager_evt_handler(dm_handle_t const *p_handle,
                                           dm_event_t const *p_event,
                                           ret_code_t event_result)
{
    APP_ERROR_CHECK(event_result);
    uint32_t err_code;
    switch (p_event->event_id)
    {
    case DM_EVT_DEVICE_CONTEXT_LOADED: // Fall through.
    case DM_EVT_SECURITY_SETUP_COMPLETE:
        m_bonded_peer_handle = (*p_handle);
        break;
#ifdef BLE_DFU_APP_SUPPORT
    case DM_EVT_LINK_SECURED:
        app_context_load(p_handle);
        break;
#endif                                            // BLE_DFU_APP_SUPPORT
    case BLE_GAP_SEC_STATUS_PASSKEY_ENTRY_FAILED: //handle passkey pairing fail event
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        APP_ERROR_CHECK(err_code);
        return NRF_SUCCESS;
    }
    }

    return NRF_SUCCESS;
}

/**@brief Function for the Device Manager initialization.
 *
 * @param[in] erase_bonds  Indicates whether bonding information should be cleared from
 *                         persistent storage during initialization of the Device Manager.
 */
static void device_manager_init(bool erase_bonds)
{
    uint32_t err_code;
    dm_init_param_t init_param = {.clear_persistent_data = erase_bonds};
    dm_application_param_t register_param;

    // Initialize peer device handle.
    err_code = dm_handle_initialize(&m_bonded_peer_handle);
    APP_ERROR_CHECK(err_code);

    // Initialize persistent storage module.
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    err_code = dm_init(&init_param);
    APP_ERROR_CHECK(err_code);

    memset(&register_param.sec_param, 0, sizeof(ble_gap_sec_params_t));

    register_param.sec_param.bond = SEC_PARAM_BOND;
    register_param.sec_param.mitm = SEC_PARAM_MITM;
    register_param.sec_param.io_caps = SEC_PARAM_IO_CAPABILITIES;
    register_param.sec_param.oob = SEC_PARAM_OOB;
    register_param.sec_param.min_key_size = SEC_PARAM_MIN_KEY_SIZE;
    register_param.sec_param.max_key_size = SEC_PARAM_MAX_KEY_SIZE;
    register_param.evt_handler = device_manager_evt_handler;
    register_param.service_type = DM_PROTOCOL_CNTXT_GATT_SRVR_ID;

    err_code = dm_register(&m_app_handle, &register_param);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(void)
{
    cherry8x16_init();

    nrf_gpio_cfg_output(LED_NUM);
    nrf_gpio_cfg_output(LED_CAPS);
    nrf_gpio_cfg_output(LED_SCLK);
}

/**@brief Function for the Power manager.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for application main entry.
 */
int main(void)
{
    uint32_t err_code;
    _Bool erase_bond = false;

    // Initialize.
    app_trace_init();
    timers_init();
    buttons_leds_init();
    ble_stack_init();
    scheduler_init();

    nrf_gpio_pin_set(row_pin_array[1]);
    erase_bond = nrf_gpio_pin_read(column_pin_array[1]);
    nrf_gpio_pin_clear(row_pin_array[1]);

    device_manager_init(erase_bond);
    gap_params_init();
    advertising_init();
    services_init();
    battery_service_init();
    conn_params_init();
    buffer_init();

    // Start execution.
    timers_start();
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);

    led_notice();

    nrf_gpio_pin_set(LED_NUM);
    m_led_state[OUTPUT_REPORT_BIT_MASK_NUM_LOCK] = true;

    // Enter main loop.
    for (;;)
    {
        app_sched_execute();
        power_manage();
    }
}

/**
 * @}
 */
