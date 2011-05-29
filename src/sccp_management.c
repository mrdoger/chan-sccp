
/*!
 * \file 	sccp_management.c
 * \brief 	SCCP Management Class
 * \author 	Marcello Ceschia <marcello [at] ceschia.de>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date: 2010-11-22 14:09:28 +0100 (Mon, 22 Nov 2010) $
 * $Revision: 2174 $  
 */
#include "config.h"
#ifdef CS_SCCP_MANAGER
#    include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2174 $")

/*!
 * \brief Show Device Description
 */
static char management_show_devices_desc[] = "Description: Lists SCCP devices in text format with details on current status.\n" "\n" "DevicelistComplete.\n" "Variables: \n" "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";

/*!
 * \brief Show Line Description
 */
static char management_show_lines_desc[] = "Description: Lists SCCP lines in text format with details on current status.\n" "\n" "LinelistComplete.\n" "Variables: \n" "  ActionID: <id>	Action ID for this transaction. Will be returned.\n";

/*!
 * \brief Restart Device Description
 */
static char management_restart_devices_desc[] = "Description: restart a given device\n" "\n" "Variables:\n" "   Devicename: Name of device to restart\n";

/*!
 * \brief Add Device Line Description
 */
static char management_show_device_add_line_desc[] = "Description: Lists SCCP devices in text format with details on current status.\n" "\n" "DevicelistComplete.\n" "Variables: \n" "  Devicename: Name of device to restart.\n" "  Linename: Name of line";

/*!
 * \brief Show Device Update Description
 */
static char management_device_update_desc[] = "Description: restart a given device\n" "\n" "Variables:\n" "   Devicename: Name of device\n";

/*!
 * \brief Show Line Forward Description
 */
static char management_line_fwd_update_desc[] = "Description: update forward status for line\n" "\n" "Variables:\n" "  Linename: Name of line\n" "  Forwardtype: type of cfwd (all | busy | noAnswer)\n" "  Number: number to forward calls (optional)";

static int sccp_manager_show_devices(struct mansession *s, const struct message *m);

static int sccp_manager_show_lines(struct mansession *s, const struct message *m);

static int sccp_manager_restart_device(struct mansession *s, const struct message *m);

static int sccp_manager_device_add_line(struct mansession *s, const struct message *m);

static int sccp_manager_device_update(struct mansession *s, const struct message *m);

static int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m);
static int sccp_manager_startCall(struct mansession *s, const struct message *m);
static int sccp_manager_answerCall(struct mansession *s, const struct message *m);
static int sccp_manager_hangupCall(struct mansession *s, const struct message *m);
static int sccp_manager_holdCall(struct mansession *s, const struct message *m);

/*!
 * \brief Register management commands
 */
int sccp_register_management(void)
{
	int result;

	/* Register manager commands */
#    if ASTERISK_VERSION_NUMBER < 10600
#        define _MAN_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG
#    else
#        define _MAN_FLAGS	EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING
#    endif
	result = ast_manager_register2("SCCPListDevices", _MAN_FLAGS, sccp_manager_show_devices, "List SCCP devices (text format)", management_show_devices_desc);
	result |= ast_manager_register2("SCCPListLines", _MAN_FLAGS, sccp_manager_show_lines, "List SCCP lines (text format)", management_show_lines_desc);
	result |= ast_manager_register2("SCCPDeviceRestart", _MAN_FLAGS, sccp_manager_restart_device, "Restart a given device", management_restart_devices_desc);
	result |= ast_manager_register2("SCCPDeviceAddLine", _MAN_FLAGS, sccp_manager_device_add_line, "add a line to device", management_show_device_add_line_desc);
	result |= ast_manager_register2("SCCPDeviceUpdate", _MAN_FLAGS, sccp_manager_device_update, "add a line to device", management_device_update_desc);
	result |= ast_manager_register2("SCCPLineForwardUpdate", _MAN_FLAGS, sccp_manager_line_fwd_update, "add a line to device", management_line_fwd_update_desc);
	result |= ast_manager_register2("SCCPStartCall", _MAN_FLAGS, sccp_manager_startCall, "start a new call on device", ""); /*!< \todo add description for ami */
	result |= ast_manager_register2("SCCPAnswerCall", _MAN_FLAGS, sccp_manager_answerCall, "answer a ringin channel", ""); /*!< \todo add description for ami */
	result |= ast_manager_register2("SCCPHangupCall", _MAN_FLAGS, sccp_manager_hangupCall, "hangup a channel", ""); /*!< \todo add description for ami */
	result |= ast_manager_register2("SCCPHoldCall", _MAN_FLAGS, sccp_manager_holdCall, "hold/unhold a call", ""); /*!< \todo add description for ami */
#    undef _MAN_FLAGS
	return result;
}

/*!
 * \brief Unregister management commands
 */
int sccp_unregister_management(void)
{
	int result;

	result = ast_manager_unregister("SCCPListDevices");
	result |= ast_manager_unregister("SCCPDeviceRestart");
	result |= ast_manager_unregister("SCCPDeviceAddLine");
	result |= ast_manager_unregister("SCCPDeviceUpdate");
	result |= ast_manager_unregister("SCCPLineForwardUpdate");
	result |= ast_manager_unregister("SCCPStartCall");
	result |= ast_manager_unregister("SCCPAnswerCall");
	result |= ast_manager_unregister("SCCPHangupCall");
	result |= ast_manager_unregister("SCCPHoldCall");
	return result;
}

/*!
 * \brief Show Devices Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- devices
 */
int sccp_manager_show_devices(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");

	sccp_device_t *device;

	char idtext[256] = "";

	int total = 0;

	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

	pbxman_send_listack(s, m, "Device status list will follow", "start");
	/* List the peers in separate manager events */
	SCCP_RWLIST_RDLOCK(&GLOB(devices));
	SCCP_RWLIST_TRAVERSE(&GLOB(devices), device, list) {
		astman_append(s, "Devicename: %s\r\n", device->id);
		total++;
	}

	SCCP_RWLIST_UNLOCK(&GLOB(devices));

	/* Send final confirmation */
	astman_append(s, "Event: SCCPListDevicesComplete\r\n" "EventList: Complete\r\n" "ListItems: %d\r\n" "\r\n", total);
	return 0;
}

/*!
 * \brief Show Lines Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 * 
 * \lock
 * 	- lines
 */
int sccp_manager_show_lines(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");

	sccp_line_t *line;

	char idtext[256] = "";

	int total = 0;

	snprintf(idtext, sizeof(idtext), "ActionID: %s\r\n", id);

	pbxman_send_listack(s, m, "Device status list will follow", "start");
	/* List the peers in separate manager events */
	SCCP_RWLIST_RDLOCK(&GLOB(lines));
	SCCP_RWLIST_TRAVERSE(&GLOB(lines), line, list) {
		astman_append(s, "Line id: %s\r\n", line->id);
		astman_append(s, "Line name: %s\r\n", line->name);
		astman_append(s, "Line description: %s\r\n", line->description);
		total++;
	}

	SCCP_RWLIST_UNLOCK(&GLOB(lines));

	/* Send final confirmation */
	astman_append(s, "Event: SCCPListLinesComplete\r\n" "EventList: Complete\r\n" "ListItems: %d\r\n" "\r\n", total);
	return 0;
}

/*!
 * \brief Restart Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
int sccp_manager_restart_device(struct mansession *s, const struct message *m)
{
//      sccp_list_t     *hintList = NULL;
	sccp_device_t *d;

	const char *fn = astman_get_header(m, "Devicename");

	const char *type = astman_get_header(m, "Type");

	ast_log(LOG_WARNING, "Attempt to get device %s\n", fn);
	if (sccp_strlen_zero(fn)) {
		astman_send_error(s, m, "Please specify the name of device to be reset");
		return 0;
	}

	ast_log(LOG_WARNING, "Type of Restart ([quick|reset] or [full|restart]) %s\n", fn);
	if (sccp_strlen_zero(fn)) {
		ast_log(LOG_WARNING, "Type not specified, using quick");
		type = "reset";
	}

	d = sccp_device_find_byid(fn, FALSE);
	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!d->session) {
		astman_send_error(s, m, "Device not registered");
		return 0;
	}

	if (!strncasecmp(type, "quick", 5) || !strncasecmp(type, "full", 5)) {
		sccp_device_sendReset(d, SKINNY_DEVICE_RESET);
	} else {
		sccp_device_sendReset(d, SKINNY_DEVICE_RESTART);
	}

	//astman_start_ack(s, m);
	astman_append(s, "Send %s restart to device %s\r\n", type, fn);
	astman_append(s, "\r\n");

	return 0;
}

/*!
 * \brief Add Device Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_device_add_line(struct mansession *s, const struct message *m)
{
	sccp_device_t *d;

	sccp_line_t *line;

	const char *deviceName = astman_get_header(m, "Devicename");

	const char *lineName = astman_get_header(m, "Linename");

	ast_log(LOG_WARNING, "Attempt to get device %s\n", deviceName);

	if (sccp_strlen_zero(deviceName)) {
		astman_send_error(s, m, "Please specify the name of device");
		return 0;
	}

	if (sccp_strlen_zero(lineName)) {
		astman_send_error(s, m, "Please specify the name of line to be added");
		return 0;
	}

	d = sccp_device_find_byid(deviceName, FALSE);

	line = sccp_line_find_byname_wo(lineName, TRUE);

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!line) {
		astman_send_error(s, m, "Line not found");
		return 0;
	}
#    ifdef CS_DYNAMIC_CONFIG
	sccp_config_addButton(d, -1, LINE, line->name, NULL, NULL);
#    else
	// last 0 should be replaced but the number of device->buttonconfig->instance + 1
	sccp_config_addLine(d, line->name, 0, 0);
#    endif

	astman_append(s, "Done\r\n");
	astman_append(s, "\r\n");

	return 0;
}

/*!
 * \brief Update Line Forward Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * // \todo TODO This function does not do anything. Has the implementation of this function moved somewhere else ?
 * 
 * \called_from_asterisk
 */
int sccp_manager_line_fwd_update(struct mansession *s, const struct message *m)
{
	sccp_line_t *line;

	sccp_device_t *device;

	sccp_linedevices_t *linedevice;

	const char *deviceId = astman_get_header(m, "DeviceId");

	const char *lineName = astman_get_header(m, "Linename");

	const char *forwardType = astman_get_header(m, "Forwardtype");

	const char *Disable = astman_get_header(m, "Disable");

	const char *number = astman_get_header(m, "Number");

	//char *fwdNumber = (char *)number;

	device = sccp_device_find_byid(deviceId, TRUE);
	if (!device) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	line = sccp_line_find_byname_wo(lineName, TRUE);

	if (!line) {
		astman_send_error(s, m, "Line not found");
		return 0;
	}

	if (line->devices.size > 1) {
		astman_send_error(s, m, "Callforwarding on shared lines is not supported at the moment");
		return 0;
	}

	if (!forwardType) {
		astman_send_error(s, m, "Forwardtype is not optional [all | busy]");	/* NoAnswer to be added later on */
		return 0;
	}

	if (!Disable) {
		Disable = "no";
	}

	if (line) {
		linedevice = sccp_util_getDeviceConfiguration(device, line);
		if (linedevice) {
			if (!sccp_strcasecmp("all", forwardType)) {
				if (!sccp_strcasecmp("no", Disable)) {
					linedevice->cfwdAll.enabled = 0;
				} else {
					linedevice->cfwdAll.enabled = 1;
				}
				sccp_copy_string(linedevice->cfwdAll.number, number, strlen(number));
			} else if (!sccp_strcasecmp("busy", forwardType)) {
				if (!sccp_strcasecmp("no", Disable)) {
					linedevice->cfwdBusy.enabled = 0;
				} else {
					linedevice->cfwdBusy.enabled = 1;
				}
				sccp_copy_string(linedevice->cfwdBusy.number, number, strlen(number));

/*! \todo cfwdNoAnswer is not implemented yet in sccp_linedevices_t */

/*			} else if (!sccp_strcasecmp("noAnswer", forwardType)) {
				if (!strcasecmp(Disable,"no")) {
					linedevice->cfwdNoAnswer.enabled=0;
				} else {
					linedevice->cfwdNoAnswer.enabled=1;
				}
				sccp_copy_string(linedevice->cfwdNoAnswer.number,number,strlen(number));
				astman_send_error(s,m, "noAnswer is not handled for the moment");*/
			}
		}
	}

	return 0;
}

/*!
 * \brief Update Device Command
 * \param s Management Session
 * \param m Message 
 * \return Success as int
 * 
 * \called_from_asterisk
 */
static int sccp_manager_device_update(struct mansession *s, const struct message *m)
{
	sccp_device_t *d;

	const char *deviceName = astman_get_header(m, "Devicename");

	if (sccp_strlen_zero(deviceName)) {
		astman_send_error(s, m, "Please specify the name of device");
		return 0;
	}

	d = sccp_device_find_byid(deviceName, FALSE);

	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}

	if (!d->session) {
		astman_send_error(s, m, "Device not active");
		return 0;
	}

	sccp_handle_soft_key_template_req(d->session, d, NULL);

	sccp_handle_button_template_req(d->session, d, NULL);

	astman_append(s, "Done\r\n");
	astman_append(s, "\r\n");

	return 0;
}


static int sccp_manager_startCall(struct mansession *s, const struct message *m)
{
	sccp_device_t *d;
	sccp_line_t *line = NULL;
	
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *lineName = astman_get_header(m, "Linename");
	const char *number = astman_get_header(m, "number");

	d = sccp_device_find_byid(deviceName, FALSE);
	if (!d) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}
	
	if(!lineName){
		if (d && d->defaultLineInstance > 0){
			line = sccp_line_find_byid(d, d->defaultLineInstance);
		} else {
			line = sccp_dev_get_activeline(d);
		}
	}else{
		line = sccp_line_find_byname_wo(lineName, FALSE);
	}
	
	if (!line) {
		astman_send_error(s, m, "Line not found");
		return 0;
	}
	


	sccp_channel_newcall(line, d, (char *)number, SKINNY_CALLTYPE_OUTBOUND);
	return 0;
}

static int sccp_manager_answerCall(struct mansession *s, const struct message *m)
{
	sccp_device_t *device;
	sccp_channel_t *c;
	
	const char *deviceName = astman_get_header(m, "Devicename");
	const char *channelId = astman_get_header(m, "channelId");

	device = sccp_device_find_byid(deviceName, FALSE);
	if (!device) {
		astman_send_error(s, m, "Device not found");
		return 0;
	}
	
	c = sccp_channel_find_byid_locked(atoi(channelId));
	
	if(!c){
		astman_send_error(s, m, "Call not found\r\n");
		return 0;
	}
	
	if(c->state != SCCP_CHANNELSTATE_RINGING){
		astman_send_error(s, m, "Call is not ringin\r\n");
		return 0;
	}
	
	astman_append(s, "Answering channel '%s'\r\n", channelId);

	sccp_channel_answer_locked(device, c);
	sccp_channel_unlock(c);


	return 0;
}

static int sccp_manager_hangupCall(struct mansession *s, const struct message *m)
{
	sccp_channel_t *c;
  
	const char *channelId = astman_get_header(m, "channelId");

	c = sccp_channel_find_byid_locked(atoi(channelId));
	if(!c){
		astman_send_error(s, m, "Call not found\r\n");
		return 0;
	}
	
	astman_append(s, "Hangup call '%s'\r\n", channelId);
	sccp_channel_endcall_locked(c);
	sccp_channel_unlock(c);

	return 0;
}


static int sccp_manager_holdCall(struct mansession *s, const struct message *m)
{
	sccp_channel_t *c;
	const char *channelId = astman_get_header(m, "channelId");
	const char *hold = astman_get_header(m, "hold");

	c = sccp_channel_find_byid_locked(atoi(channelId));
	if(!c){
		astman_send_error(s, m, "Call not found\r\n" );
		return 0;
	}
	if (!sccp_strcasecmp("on", hold)) {						/* check to see if enable hold */
		astman_append(s, "Put channel '%s' on hold\n", channelId);
		sccp_channel_hold_locked(c);
		sccp_channel_unlock(c);
	} else if (!sccp_strcasecmp("off", hold)) {					/* check to see if disable hold */
		astman_append(s, "remove channel '%s' from hold\n", channelId);
		sccp_channel_resume_locked(c->device, c, FALSE);
		sccp_channel_unlock(c);
	} else{
		astman_send_error(s, m, "Invalid value for park, us on or off only\r\n" );
	}
	return 0;
}

#endif
