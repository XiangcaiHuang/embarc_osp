/* ------------------------------------------
 * Copyright (c) 2017, Synopsys, Inc. All rights reserved.

 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:

 * 1) Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.

 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.

 * 3) Neither the name of the Synopsys, Inc., nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * \version 2017.03
 * \date 2016-07-25
 * \author Xinyi Zhao(xinyi@synopsys.com)
--------------------------------------------- */

/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Julien Vermillard - initial implementation
 *    Fabien Fleutot - Please refer to git log
 *    David Navarro, Intel Corporation - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *
 *******************************************************************************/

/*
 * This object is single instance only, and provide firmware upgrade functionality.
 * Object ID is 5.
 */

/*
 * resources:
 * 0 package                   write
 * 1 package url               write
 * 2 update                    exec
 * 3 state                     read
 * 4 update supported objects  read/write
 * 5 update result             read
 */

#include "liblwm2m.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "embARC.h"
#include "embARC_debug.h"
#include "lwm2mclient.h"

// ---- private object "Firmware" specific defines ----
// Resource Id's:
#define RES_M_PACKAGE                   0
#define RES_M_PACKAGE_URI               1
#define RES_M_UPDATE                    2
#define RES_M_STATE                     3
#define RES_O_UPDATE_SUPPORTED_OPJECTS  4
#define RES_M_UPDATE_RESULT             5
#define RES_O_PKG_NAME                  6
#define RES_O_PKG_VERSION               7

extern void task_lwm2m_update(void *par);

FIL fp;

typedef struct
{
	uint8_t state;
	uint8_t supported;
	uint8_t result;
} firmware_data_t;

static uint8_t prv_firmware_read(uint16_t instanceId, int * numDataP, lwm2m_tlv_t ** dataArrayP,
				lwm2m_object_t * objectP)
{
	int i;
	uint8_t result;
	firmware_data_t * data = (firmware_data_t*)(objectP->userData);

	// this is a single instance object
	if (instanceId != 0) {
		return COAP_404_NOT_FOUND;
	}

	// is the server asking for the full object ?
	if (*numDataP == 0) {
		*dataArrayP = lwm2m_tlv_new(3);
		if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
		*numDataP = 3;
		(*dataArrayP)[0].id = 3;
		(*dataArrayP)[1].id = 4;
		(*dataArrayP)[2].id = 5;
	}

	i = 0;
	do {
		switch ((*dataArrayP)[i].id) {
			case RES_M_PACKAGE:
			case RES_M_PACKAGE_URI:
			case RES_M_UPDATE:
				result = COAP_405_METHOD_NOT_ALLOWED;
				break;

			case RES_M_STATE:
				// firmware update state (int)
				lwm2m_tlv_encode_int(data->state, *dataArrayP + i);
				(*dataArrayP)[i].type = LWM2M_TYPE_RESOURCE;

				if (0 != (*dataArrayP)[i].length) result = COAP_205_CONTENT;
				else result = COAP_500_INTERNAL_SERVER_ERROR;

				break;

			case RES_O_UPDATE_SUPPORTED_OPJECTS:
				lwm2m_tlv_encode_int(data->supported, *dataArrayP + i);
				(*dataArrayP)[i].type = LWM2M_TYPE_RESOURCE;

				if (0 != (*dataArrayP)[i].length) result = COAP_205_CONTENT;
				else result = COAP_500_INTERNAL_SERVER_ERROR;

				break;

			case RES_M_UPDATE_RESULT:
				lwm2m_tlv_encode_int(data->result, *dataArrayP + i);
				(*dataArrayP)[i].type = LWM2M_TYPE_RESOURCE;

				if (0 != (*dataArrayP)[i].length) result = COAP_205_CONTENT;
				else result = COAP_500_INTERNAL_SERVER_ERROR;

				break;

			default:
				result = COAP_404_NOT_FOUND;
		}

		i++;
	} while (i < *numDataP && result == COAP_205_CONTENT);

	return result;
}

static uint8_t prv_firmware_write(uint16_t instanceId, int numData, lwm2m_tlv_t * dataArray,
				lwm2m_object_t * objectP)
{
	int i;
	bool bvalue;
	uint8_t result;
	firmware_data_t * data = (firmware_data_t*)(objectP->userData);
	UINT cnt;
	FRESULT err;

	// this is a single instance object
	if (instanceId != 0) {
		return COAP_404_NOT_FOUND;
	}

	i = 0;

	do {
		switch (dataArray[i].id) {
			case RES_M_PACKAGE:
				// inline firmware binary

				if (data->state == 1 || data->state == 3) {
					if ((err = f_open(&fp, "boot.hex", FA_WRITE | FA_CREATE_ALWAYS))) {
						xprintf("err is : %d\n",err);
						break;
					}

					f_write(&fp, dataArray->value, dataArray->length, &cnt);
					data->state = 2;
				} else {
					f_write(&fp, dataArray->value, dataArray->length, &cnt);
				}
				if (cnt != 256) {
					data->state = 3;
					f_close(&fp);
				}

				result = COAP_204_CHANGED;
				break;

			case RES_M_PACKAGE_URI:
				// URL for download the firmware
				result = COAP_204_CHANGED;
				break;
			case RES_O_UPDATE_SUPPORTED_OPJECTS:
				if (lwm2m_tlv_decode_bool(&dataArray[i], &bvalue)) {
					data->supported = bvalue ? 1 : 0;
					result = COAP_204_CHANGED;
				} else {
					result = COAP_400_BAD_REQUEST;
				}
				break;

			default:
				result = COAP_405_METHOD_NOT_ALLOWED;
		}

		i++;
	} while (i < numData && result == COAP_204_CHANGED);

	return result;
}

static uint8_t prv_firmware_execute(uint16_t instanceId, uint16_t resourceId, uint8_t * buffer,
				int length, lwm2m_object_t * objectP)
{
	firmware_data_t * data = (firmware_data_t*)(objectP->userData);

	// this is a single instance object
	if (instanceId != 0) {
		return COAP_404_NOT_FOUND;
	}

	if (length != 0) return COAP_400_BAD_REQUEST;

	// for execute callback, resId is always set.
	switch (resourceId) {
		case RES_M_UPDATE:
			if (data->state == 3) {
				EMBARC_PRINTF("\n\t FIRMWARE UPDATE\r\n\n");

				// data->state = 1;
				g_reboot = 1;
				return COAP_204_CHANGED;
			} else {
				// firmware update already running
				return COAP_400_BAD_REQUEST;
			}
		default:
			return COAP_405_METHOD_NOT_ALLOWED;
	}
}

static void prv_firmware_close(lwm2m_object_t * objectP)
{
	if (NULL != objectP->userData) {
		lwm2m_free(objectP->userData);
		objectP->userData = NULL;
	}
	if (NULL != objectP->instanceList) {
		lwm2m_free(objectP->instanceList);
		objectP->instanceList = NULL;
	}
}

void display_firmware_object(lwm2m_object_t * object)
{
#ifdef WITH_LOGS
	firmware_data_t * data = (firmware_data_t *)object->userData;
	EMBARC_PRINTF("  /%u: Firmware object:\r\n", object->objID);
	if (NULL != data) {
		EMBARC_PRINTF("    state: %u, supported: %u, result: %u\r\n",
				data->state, data->supported, data->result);
	}
#endif
}


lwm2m_object_t * get_object_firmware(void)
{
	/*
	 * The get_object_firmware function create the object itself and return a pointer to the structure that represent it.
	 */
	lwm2m_object_t * obj;

	obj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

	if (NULL != obj) {
		memset(obj, 0, sizeof(lwm2m_object_t));

		/*
		 * It assigns its unique ID
		 * The 5 is the standard ID for the optional object "Object firmware".
		 */
		obj->objID = LWM2M_FIRMWARE_UPDATE_OBJECT_ID;

		/*
		 * and its unique instance
		 *
		 */
		obj->instanceList = (lwm2m_list_t *)lwm2m_malloc(sizeof(lwm2m_list_t));
		if (NULL != obj->instanceList) {
			memset(obj->instanceList, 0, sizeof(lwm2m_list_t));
		} else {
			lwm2m_free(obj);
			return NULL;
		}

		/*
		 * And the private function that will access the object.
		 * Those function will be called when a read/write/execute query is made by the server. In fact the library don't need to
		 * know the resources of the object, only the server does.
		 */
		obj->readFunc    = prv_firmware_read;
		obj->writeFunc   = prv_firmware_write;
		obj->executeFunc = prv_firmware_execute;
		obj->closeFunc   = prv_firmware_close;
		obj->userData    = lwm2m_malloc(sizeof(firmware_data_t));

		/*
		 * Also some user data can be stored in the object with a private structure containing the needed variables
		 */
		if (NULL != obj->userData) {
			((firmware_data_t*)obj->userData)->state = 1;
			((firmware_data_t*)obj->userData)->supported = 0;
			((firmware_data_t*)obj->userData)->result = 0;
		} else {
			lwm2m_free(obj);
			obj = NULL;
		}
	}

	return obj;
}
