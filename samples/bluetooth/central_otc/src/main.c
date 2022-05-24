/** @file
 *  @brief Bluetooth Object Transfer Client Sample
 *
 * Copyright (c) 2020-2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <sys/byteorder.h>
#include <bluetooth/scan.h>
#include <dk_buttons_and_leds.h>
#include <bluetooth/services/ots.h>

#define BT_UUID_SERVICE_TO_SCAN BT_UUID_OTS
#define BT_OTS_NAME_MAX_SIZE 127
#define FIRST_HANDLE			0x0001
#define LAST_HANDLE			0xFFFF
#define OBJ_MAX_SIZE 102400
#define	BT_GATT_OTS_OLCP_RES_OUT_OF_BONDS 0x05

struct bt_ots_client otc;
static struct bt_ots_client_cb otc_cb;
unsigned char bwData[OBJ_MAX_SIZE] = {0};
bool first_selected = false;
void on_obj_selected(struct bt_ots_client *ots_inst,
			     struct bt_conn *conn, int err);

void on_obj_metadata_read(struct bt_ots_client *ots_inst,
				  struct bt_conn *conn, int err,
				  uint8_t metadata_read);
                  
int on_obj_data_read(struct bt_ots_client *ots_inst,
			     struct bt_conn *conn, uint32_t offset,
			     uint32_t len, uint8_t *data_p, bool is_complete);

NET_BUF_SIMPLE_DEFINE_STATIC(otc_obj_buf, CONFIG_BT_BUF_ACL_RX_SIZE*4);
static int16_t otc_handles_assign(struct bt_gatt_dm *dm);
static void start_scan(void);
static struct bt_conn *default_conn;
static void print_hex_number(uint8_t *num, size_t len)
{
	printk("0x");
	for (int i = 0; i < len; i++) {
		printk("%02x ", num[i]);
	}
	printk("\n");
}

static void discovery_complete(struct bt_gatt_dm *dm,
			       void *context)
{
	printk("Service discovery completed\n");
	bt_gatt_dm_data_print(dm);
	/* see if dst has already in our stored tag */
	otc_handles_assign(dm);
	bt_gatt_dm_data_release(dm);
}

static void discovery_service_not_found(struct bt_conn *conn,
					void *context)
{
	printk("OTS Service not found\n");
}

static void discovery_error(struct bt_conn *conn,
			    int err,
			    void *context)
{
	printk("Error while discovering OTS service GATT database: (%d)\n", err);
}
struct bt_gatt_dm_cb discovery_cb = {
	.completed         = discovery_complete,
	.service_not_found = discovery_service_not_found,
	.error_found       = discovery_error,
};
static void gatt_discover(struct bt_conn *conn)
{
	int err;

	if (conn != default_conn) {
		return;
	}

	err = bt_gatt_dm_start(conn,
			       BT_UUID_OTS,
			       &discovery_cb,
			       NULL);
	if (err) {
		printk("could not start the discovery procedure, error "
			"code: %d\n", err);
	}
}
static void start_scan(void)
{
	int err;
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning OTS TAG failed to start (err %d)\n",
			err);
	}
	
	printk("Scanning successfully started\n");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (%u)\n", addr, err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	if (conn != default_conn) {
		return;
	}

	printk("Connected: %s\n", addr);
    gatt_discover(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (conn != default_conn) {
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};
static void scan_filter_no_match(struct bt_scan_device_info *device_info,
				 bool connectable)
{
	int err;
	struct bt_conn *conn;
	char addr[BT_ADDR_LE_STR_LEN];

	if (device_info->recv_info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
		printk("Direct advertising received from %s\n", addr);

		err = bt_conn_le_create(device_info->recv_info->addr,
					BT_CONN_LE_CREATE_CONN,
					device_info->conn_param, &conn);

		if (!err) {
			default_conn = bt_conn_ref(conn);
			bt_conn_unref(conn);
		}
	}
}
static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
	printk("filters %d, %d, %d, %d, %d, %d\n", 
		filter_match->name.match, filter_match->addr.match, filter_match->uuid.match,
		filter_match->appearance.match, filter_match->short_name.match, filter_match->manufacturer_data.match);
	printk("Filters matched. Address: %s connectable: %d\n",
		addr, connectable);
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	printk("scan Connecting failed\n");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	printk("found %s\n", addr);
	default_conn = bt_conn_ref(conn);
}
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match,
		scan_connecting_error, scan_connecting);
        
static int scan_init(void)
{
	int err;
	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
	};
	uint8_t mode = BT_SCAN_UUID_FILTER;
	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_SERVICE_TO_SCAN);
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);
		return err;
	}

	err = bt_scan_filter_enable(mode, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
		return err;
	}

	printk("Scan module initialized\n");
	return err;
}

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;
	uint32_t size_to_write;
	int err;
    if (button & DK_BTN1_MSK){
        printk("select OTS object\n");
		if(!first_selected) {
			err = bt_ots_client_select_first(&otc, default_conn);
			first_selected = true;
		} else {
			err = bt_ots_client_select_next(&otc, default_conn);
		}
		if (err) {
			printk("Failed to select object\n");
			return;
		}
		printk("Selecting object succeeded\n");
    }
    if (button & DK_BTN2_MSK){
        printk("read OTS object meta\n");
		err = bt_ots_client_read_object_metadata(&otc, default_conn,
						 BT_OTS_METADATA_REQ_ALL);
		if (err) {
			printk("Failed to read object metadata\n");
			return;
		}
    }
    if (button & DK_BTN3_MSK) {
		size_to_write = otc.cur_object.size.alloc-otc.cur_object.size.cur;
        printk("write OTS object len %d\n", size_to_write);
		for(uint32_t idx=0;idx<OBJ_MAX_SIZE;idx++) {
			bwData[idx]= 255 - (idx % 256);
		}
		err = bt_ots_client_write_object_data(&otc, default_conn, bwData, size_to_write, 0,1);
		if (err) {
			printk("Failed to write object\n");
			return;
		}		
    }
    if (button & DK_BTN4_MSK){
        printk("read OTS object\n");
		err = bt_ots_client_read_object_data(&otc, default_conn);
		if (err) {
			printk("Failed to read object %d\n", err);
			return;
		}
    }
}

void on_obj_selected(struct bt_ots_client *ots_inst,
			     struct bt_conn *conn, int err) 
{
	printk("Current object selected\n");
	/* TODO: Read metadata here? */
	/* For now: Left to the application */
	if(err == BT_GATT_OTS_OLCP_RES_OUT_OF_BONDS) {
		printk("BT_GATT_OTS_OLCP_RES_OUT_OF_BONDS %d\n", err);
		bt_ots_client_select_first(&otc, default_conn);
	}
	/* Only one object at a time is selected in OTS */
	/* When the selected callback comes, a new object is selected */
	/* Reset the object buffer */
	net_buf_simple_reset(&otc_obj_buf);

}

/* TODO: Merge the object callback functions into one */
/* Use a notion of the "active" object, as done in mpl.c, for tracking  */
int on_obj_data_read(struct bt_ots_client *ots_inst,
			     struct bt_conn *conn, uint32_t offset,
			     uint32_t len, uint8_t *data_p, bool is_complete)
{
	int cb_err = 0;

	printk("Received OTS Object content, %i bytes at offset %i\n",
		len, offset);

	print_hex_number(data_p, len);

	if (len > net_buf_simple_tailroom(&otc_obj_buf)) {
		printk("Can not fit whole object\n");
		cb_err = -EMSGSIZE;
	}

	net_buf_simple_add_mem(&otc_obj_buf, data_p,
			       MIN(net_buf_simple_tailroom(&otc_obj_buf), len));

	if (is_complete) {
		printk(" object received\n");

		/* Reset buf in case the same object is read again without */
		/* calling select in between */
		net_buf_simple_reset(&otc_obj_buf);
		return BT_OTS_STOP;
	}

	return BT_OTS_CONTINUE;
}
void on_obj_metadata_read(struct bt_ots_client *ots_inst,
					struct bt_conn *conn, int err,
					uint8_t metadata_read)
{
	printk("Object's meta data:\n");
	printk("\tCurrent size\t:%u", ots_inst->cur_object.size.cur);
	printk("\tAlloc size\t:%u", ots_inst->cur_object.size.alloc);

	if (ots_inst->cur_object.size.cur > otc_obj_buf.size) {
		printk("Object larger than allocated buffer\n");
	}

	bt_ots_metadata_display(&ots_inst->cur_object, 1);

}
static int16_t otc_handles_assign(struct bt_gatt_dm *dm)
{
	const struct bt_gatt_dm_attr *gatt_service_attr =
			bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service =
			bt_gatt_dm_attr_service_val(gatt_service_attr);
	const struct bt_gatt_dm_attr *gatt_chrc;
	const struct bt_gatt_dm_attr *gatt_desc;
    struct bt_gatt_subscribe_params *sub_params_1 = NULL;
	struct bt_gatt_subscribe_params *sub_params_2 = NULL;

	if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_OTS)) {
		printk("OTS SERVICE UUID not match\n");
		return -ENOTSUP;
	}
	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_FEATURE);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_FEATURE characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_FEATURE);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_FEATURE value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.feature_handle = gatt_desc->handle;
    
    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_NAME);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_NAME characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_NAME);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_NAME value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.obj_name_handle = gatt_desc->handle;
    
    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_TYPE);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_TYPE characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_TYPE);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_TYPE value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.obj_type_handle = gatt_desc->handle;    

    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_SIZE);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_SIZE characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_SIZE);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_SIZE value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.obj_size_handle = gatt_desc->handle;

    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_ID);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_ID characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_ID);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_ID value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.obj_id_handle = gatt_desc->handle;
    
    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_PROPERTIES);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_PROPERTIES characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_PROPERTIES);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_PROPERTIES value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.obj_properties_handle = gatt_desc->handle;
    
    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_ACTION_CP);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_ACTION_CP characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_ACTION_CP);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_ACTION_CP value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.oacp_handle = gatt_desc->handle;
    
    gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_OTS_LIST_CP);
	if (!gatt_chrc) {
		printk("Missing BT_UUID_OTS_LIST_CP characteristic.\n");
		return -EINVAL;
	}
	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_OTS_LIST_CP);
	if (!gatt_desc) {
		printk("Missing BT_UUID_OTS_LIST_CP value descriptor in characteristic.\n");
		return -EINVAL;
	}
	otc.olcp_handle = gatt_desc->handle;
    
	sub_params_1 = &otc.oacp_sub_params;
	sub_params_1->disc_params = &otc.oacp_sub_disc_params;
	if (sub_params_1) {
		/* With ccc_handle == 0 it will use auto discovery */
		sub_params_1->ccc_handle = 0;
		sub_params_1->end_handle = otc.end_handle;
		sub_params_1->value = BT_GATT_CCC_INDICATE;
		sub_params_1->value_handle = otc.oacp_handle;
		sub_params_1->notify = bt_ots_client_indicate_handler;
		bt_gatt_subscribe(bt_gatt_dm_conn_get(dm), sub_params_1);
	}
	sub_params_2 = &otc.olcp_sub_params;
	sub_params_2->disc_params = &otc.olcp_sub_disc_params;
	if (sub_params_2) {
		/* With ccc_handle == 0 it will use auto discovery */
		sub_params_2->ccc_handle = 0;
		sub_params_2->end_handle = otc.end_handle;
		sub_params_2->value = BT_GATT_CCC_INDICATE;
		sub_params_2->value_handle = otc.olcp_handle;
		sub_params_2->notify = bt_ots_client_indicate_handler;
		bt_gatt_subscribe(bt_gatt_dm_conn_get(dm), sub_params_2);
	}
	/* No more attributes found */
	bt_ots_client_register(&otc);
	printk("Setup complete for OTS\n");
	return 0;
}

void bt_otc_init(void)
{
    otc_cb.obj_data_read  = on_obj_data_read;
	otc_cb.obj_selected = on_obj_selected;
	otc_cb.obj_metadata_read = on_obj_metadata_read;
	otc.start_handle = FIRST_HANDLE;
	otc.end_handle = LAST_HANDLE;
	printk("Current object selected callback: %p\n", otc_cb.obj_selected);
	printk("Content callback: %p\n", otc_cb.obj_data_read);
	printk("Metadata callback: %p\n", otc_cb.obj_metadata_read);
	otc.cb = &otc_cb;
}
void main(void)
{
	int err;
	err = dk_buttons_init(button_handler);

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}
    scan_init();
    bt_otc_init();
	printk("Bluetooth OTS client sample running\n");

	start_scan();
}
