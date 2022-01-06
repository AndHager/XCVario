/**
 *
 * UbloxGNSSdecode.cpp
 * This software parses a data stream to extract UBX NAV-PVT frame information. Key informationtion can be accessed with getGNSSData() using GNSS_DATA_T type.
 * This software also captures NMEA sentences and return them through the exchange buffer so that they can be further processed by the calling software.
 * This feature allows to mix UBX and NMEA sentences
 * Aurelien & Jean-Luc Derouineau 2022
 *
 */

#include <stdint.h>
#include <endian.h>
#include <string.h>

#include <esp_log.h>
#include <logdef.h>

#include "UbloxGNSSdecode.h"

// NAV_PVT structure definition
struct NAV_PVT_PAYLOAD { // offset in UBX frame:
	uint32_t iTOW;         // 00: GPS time of week of the navigation epoch (ms)
	uint16_t year;         // 04: Year (UTC)
	uint8_t month;         // 06: Month, range 1..12 (UTC)
	uint8_t day;           // 07: Day of month, range 1..31 (UTC)
	uint8_t hour;          // 08: Hour of day, range 0..23 (UTC)
	uint8_t minute;        // 09: Minute of hour, range 0..59 (UTC)
	uint8_t second;        // 10: Seconds of minute, range 0..60 (UTC)
	uint8_t valid;         // 11: Validity Flags (see graphic below)
	uint32_t tAcc;         // 12: Time accuracy estimate (UTC) (ns)
	int32_t nano;          // 16: Fraction of second, range -1e9 .. 1e9 (UTC) (ns)
	uint8_t fixType;       // 20: GNSSfix Type, range 0..5
	uint8_t flags;         // 21: Fix Status Flags
	uint8_t flags2;        // 22: Additional flags
	uint8_t numSV;         // 23: Number of satellites used in Nav Solution
	int32_t lon;           // 24: Longitude (deg)
	int32_t lat;           // 28: Latitude (deg)
	int32_t height;        // 32: Height above Ellipsoid (mm)
	int32_t hMSL;          // 36: Height above mean sea level (mm)
	uint32_t hAcc;         // 40: Horizontal Accuracy Estimate (mm)
	uint32_t vAcc;         // 44: Vertical Accuracy Estimate (mm)
	int32_t velN;          // 48: NED north velocity (mm/s)
	int32_t velE;          // 52: NED east velocity (mm/s)
	int32_t velD;          // 56: NED down velocity (mm/s)
	int32_t gSpeed;        // 60: Ground Speed (2-D) (mm/s)
	int32_t heading;       // 64: Heading of motion 2-D (deg)
	uint32_t sAcc;         // 68: Speed Accuracy Estimate
	uint32_t headingAcc;   // 72: Heading Accuracy Estimate
	uint16_t pDOP;         // 76: Position dilution of precision
	uint16_t flags3;       // 78: Additional flags
	uint32_t reserved0;    // 80: Reserved
	int32_t headVeh;       // 84: Heading of vehicue 2-D
	int16_t magDec; 		   // 88: Magnetic declination
	uint16_t magAcc;       // 90: Magnetic declination accuracy
} payload;

const size_t PVT_PAYLOAD_SIZE = sizeof(struct NAV_PVT_PAYLOAD);

// GNSS data from UBX sentence
struct GNSS_DATA_T GNSS_DATA;

// state machine definition 
static enum state_t {
	INIT,

	GOT_NMEA_DOLLAR,
	GOT_NMEA_FRAME,

	GOT_UBX_SYNC1,
	GOT_UBX_SYNC2,
	GOT_UBX_CLASS,
	GOT_UBX_ID,
	GOT_UBX_LENGTH1,
	GOT_UBX_LENGTH2,
	GOT_UBX_PAYLOAD,
	GOT_UBX_CHKA
} state = INIT;

char nmeaBuf[128]; // NMEA standard max size is 82 byte including termination cr lf but provision is make to allow 128 byte in case of proprietary sentences.
char chkA = 0; // checksum variables
char chkB = 0;
char chkBuf = 0;
uint8_t pos = 0; // pointer for NMEA or UBX payload

const struct GNSS_DATA_T getGNSSData() {
	return GNSS_DATA;
}

// compute checksum
void addChk(const char c) {
	chkA += c;
	chkB += chkA;
}

void processNavPvtFrame() {
	// nav fix must be 3D or 3D diff to accept new data
	GNSS_DATA.fix = false;
	if ( payload.fixType != 3 && payload.fixType != 4 ) return;

	GNSS_DATA.coordinates.latitude = payload.lat * 1e-7f;
	GNSS_DATA.coordinates.longitude = payload.lon * 1e-7f;
	GNSS_DATA.coordinates.altitude = payload.hMSL * 0.001f;
	GNSS_DATA.speed.ground = payload.gSpeed * 0.001f;
	GNSS_DATA.speed.x = payload.velN * 0.001f;
	GNSS_DATA.speed.y = payload.velE * 0.001f;
	GNSS_DATA.speed.z = payload.velD * 0.001f;
	GNSS_DATA.date = payload.day;
	GNSS_DATA.time = payload.iTOW * 0.001f;
	GNSS_DATA.fix = true;
	ESP_LOGE(FNAME, "GNSS UBX ground speed %f : ",GNSS_DATA.speed.ground);
}

// state machine to parse NMEA and UBX sentences
void parse_NMEA_UBX(const char c) {
	switch(state) {
	case INIT:
		switch(c) {
		case '$':
			pos = 0;
			nmeaBuf[pos] = c;
			pos++;
			state = GOT_NMEA_DOLLAR;
			break;
		case '!':
			pos = 0;
			nmeaBuf[pos] = c;
			pos++;
			state = GOT_NMEA_DOLLAR;
			break;
		case 0xb5:
			chkA = 0;
			chkB = 0;
			pos = 0;
			state = GOT_UBX_SYNC1;
			break;
		}
		break;

		case GOT_NMEA_DOLLAR:
			if ( ( c < 0x20 || c > 0x7e ) && c != 0x0d  && c != 0x0a ) {  // accept both closing chars, NMEA is defined with to end message by <CR><LF> == 0d 0a
				ESP_LOGE(FNAME, "Invalid NMEA character outside [20..7e] + 0x0d : %x", c);
				state = INIT;
			}
			if (pos == sizeof(nmeaBuf) - 1) {
				ESP_LOGE(FNAME, "NMEA buffer not large enough");
				state = INIT;
			}
			nmeaBuf[pos] = c;
			pos++;
			if(c == 0x0a) {
				nmeaBuf[pos] = 0;
				state = GOT_NMEA_FRAME;
			}
			break;

		case GOT_NMEA_FRAME:
			break;

		case GOT_UBX_SYNC1:
			if (c != 0x62) {
				state = INIT;
				break;
			}
			state = GOT_UBX_SYNC2;
			break;
		case GOT_UBX_SYNC2:
			if (c != 0x01) {
				ESP_LOGW(FNAME, "Unexpected UBX class");
				state = INIT;
				break;
			}
			addChk(c);
			state = GOT_UBX_CLASS;
			break;
		case GOT_UBX_CLASS:
			if (c != 0x07) {
				ESP_LOGW(FNAME, "Unexpected UBX ID");
				state = INIT;
				break;
			}
			addChk(c);
			state = GOT_UBX_ID;
			break;
		case GOT_UBX_ID:
			addChk(c);
			state = GOT_UBX_LENGTH1;
			break;
		case GOT_UBX_LENGTH1:
			addChk(c);
			state = GOT_UBX_LENGTH2;
			break;
		case GOT_UBX_LENGTH2:
			addChk(c);
			((char*)(&payload))[pos] = c;
			if (pos == PVT_PAYLOAD_SIZE - 1) {
				state = GOT_UBX_PAYLOAD;
			}
			pos++;
			break;
		case GOT_UBX_PAYLOAD:
			chkBuf = c;
			state = GOT_UBX_CHKA;
			break;
		case GOT_UBX_CHKA:
			if (chkBuf == chkA && c == chkB) {
				processNavPvtFrame();
			} else {
				ESP_LOGE(FNAME, "Checksum does not match");
			}
			state = INIT;
			break;
	}
}

bool processGNSS( SString& frame ) {
	const char* cf = frame.c_str();
	// process every frame byte through state machine
	for (size_t i = 0; i < frame.length(); i++) {
		parse_NMEA_UBX(cf[i]);
		// if NMEA sentence send it back to Router through frame
		if (state == GOT_NMEA_FRAME) {
			// ESP_LOGI(FNAME,"GOT_NMEA_FRAME return pos: %d", pos );
			frame.set(nmeaBuf, pos);
			state = INIT;
			return true;
		}
	}
	return false;
}
