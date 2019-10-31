#ifndef _NtpClientLib_h
#define _NtpClientLib_h

//#define DEBUG_NTPCLIENT //Uncomment this to enable debug messages over serial port

#include <functional>
using namespace std;
using namespace placeholders;

extern "C" {
#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/dns.h"
}

#include <TimeLib.h>


#define NETWORK_ESP8266			(100) // ESP8266 boards, not for Arduino using AT firmware
#define NETWORK_ESP32           (101) // ESP32 boards

#define DEFAULT_NTP_SERVER "pool.ntp.org" // Default international NTP server. I recommend you to select a closer server to get better accuracy
#define DEFAULT_NTP_PORT 123 // Default local udp port. Select a different one if neccesary (usually not needed)
#define DEFAULT_NTP_INTERVAL 1800 // Default sync interval 30 minutes
#define DEFAULT_NTP_SHORTINTERVAL 15 // Sync interval when sync has not been achieved. 15 seconds
#define DEFAULT_NTP_TIMEZONE 0 // Select your local time offset. 0 if UTC time has to be used
#define MIN_NTP_TIMEOUT 100 // Minumum admisible ntp timeout

#define DST_ZONE_EU             (0)
#define DST_ZONE_USA            (1)
#define DST_ZONE_COUNT          (2)
#define DEFAULT_DST_ZONE        DST_ZONE_EU

#define SERVER_NAME_LENGTH 40
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message

#ifdef ARDUINO_ARCH_ESP8266
#define NETWORK_TYPE NETWORK_ESP8266
#elif defined ARDUINO_ARCH_ESP32 || defined ESP32
#define NETWORK_TYPE NETWORK_ESP32
#endif

#if NETWORK_TYPE == NETWORK_ESP8266
#include <ESP8266WiFi.h>
#include <ESPAsyncUDP.h>
#elif NETWORK_TYPE == NETWORK_ESP32
#include <WiFi.h>
#include <AsyncUDP.h>
#else
#error "Incorrect platform. Only ARDUINO and ESP8266 MCUs are valid."
#endif // NETWORK_TYPE
#include <Ticker.h>

typedef enum NTPStatus {
	requestNTP = 2,				// running, waiting for response
	requestDNS = 1,				// running, waiting for response
	syncd = 0,					// Time synchronized correctly
	unsyncd = -1,				// Time may not be valid
	errorDNS = -2,				// DNS Error, may be network
	errorInvalidAddress = -3,	// Address not reachable
	errorTimeOutDNS = -4,		// DNS TimeOut
	errorNoResponse = -5,		// No response from server
	errorSending = -6,			// An error happened while sending the request
	errorResponse = -7,			// Wrong response received
	errorTimeOutNTP = -8		// NTP TimeOut
} NTPStatus_t; // Only for internal library use

#if defined ARDUINO_ARCH_ESP8266 || defined ARDUINO_ARCH_ESP32
#include <functional>
typedef std::function<void(NTPStatus_t)> onSyncEvent_t;
#else
typedef void(*onSyncEvent_t)(NTPStatus_t);
#endif

class NTPClient {
public:
    NTPClient ();
    ~NTPClient ();

    /**
    * Starts time synchronization.
    * @param[in] NTP server name as String.
    * @param[in] Time offset from UTC.
    * @param[in] true if this time zone has dayligth saving.
    * @param[in] Minutes offset added to hourly offset (optional).
    * @param[in] UDP connection instance (optional).
    * @param[out] true if everything went ok.
    */
    bool begin (String ntpServerName = DEFAULT_NTP_SERVER, int8_t timeOffset = DEFAULT_NTP_TIMEZONE, bool daylight = false, int8_t minutes = 0, AsyncUDP* udp_conn = NULL);
	void processStart();
    void stop ();

    bool setNtpServerName (String ntpServerName);
    bool setNtpServerName (char* ntpServerName);
    String getNtpServerName ();
    char* getNtpServerNamePtr ();

    bool setTimeZone (int8_t timeZone, int8_t minutes = 0);
    int8_t getTimeZone ();
    int8_t getTimeZoneMinutes ();

    // Sets DST zone (DST_ZONE_EU || DST_ZONE_USA).
    bool setDSTZone (uint8_t dstZone);
    uint8_t getDSTZone ();

    void setDayLight (bool daylight);
    bool getDayLight ();

    bool setInterval (int interval);
    bool setInterval (int shortInterval, int longInterval);
    int getInterval ();
    int	getShortInterval ();
    int	getLongInterval () { return getInterval (); }

	void setNextInterval(int interval);
	int getNextInterval();

    boolean setNTPTimeout (uint16_t milliseconds);
    uint16_t getNTPTimeout ();

    time_t getLastNTPSync ();

	void onNTPSyncEvent(onSyncEvent_t handler);
	String getStatusString();

    String getTimeStr () { return getTimeStr (now ()); }
    String getTimeStr (time_t moment);
    String getDateStr () { return getDateStr (now ()); }
    String getDateStr (time_t moment);
    String getTimeDateString () { return getTimeDateString (now ()); }
    String getTimeDateString (time_t moment);

    boolean isSummerTime () {
        if (_daylight)
            return isSummerTimePeriod (now ());
        else
            return false;
    }
    boolean isSummerTimePeriod (time_t moment);
    bool summertime (int year, byte month, byte day, byte hour, byte weekday, byte tzHours);

protected:
    AsyncUDP *udp;              ///< UDP connection object
    bool _daylight;             ///< Does this time zone have daylight saving?
    int8_t _timeZone = 0;       ///< Keep track of set time zone offset
    int8_t _minutesOffset = 0;   ///< Minutes offset for time zones with decimal numbers
    uint8_t _dstZone = DEFAULT_DST_ZONE; ///< Daylight save time zone
    char _ntpServerName[SERVER_NAME_LENGTH];       ///< Name of NTP server on Internet or LAN
    int _shortInterval = DEFAULT_NTP_SHORTINTERVAL;         ///< Interval to set periodic time sync until first synchronization.
    int _longInterval = DEFAULT_NTP_INTERVAL;          ///< Interval to set periodic time sync
	int _nextInterval = DEFAULT_NTP_SHORTINTERVAL;      ///< Last interval to set periodic time sync
	time_t _lastSyncd = 0;      ///< Stored time of last successful sync
    uint16_t ntpTimeout = 1500; ///< Response timeout for NTP requests
	onSyncEvent_t onSyncEvent;  ///< Event handler callback

	NTPStatus_t status = unsyncd;
	Ticker tickTimeout;
	Ticker processTimer;

	void updateStatus(NTPStatus_t newstatus);
    boolean sendNTPpacket (AsyncUDP *udp);

    /**
    * Static method for Ticker argument.
    */
	static void s_onDNSFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
	void onDNSFound(const ip_addr_t *ipaddr);
	static void ICACHE_RAM_ATTR s_onDNSTimeout(void* arg);
	void onDNSTimeout();

//	static void s_packetReceive(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
	void packetReceive(AsyncUDPPacket& packet);
	static void ICACHE_RAM_ATTR s_onNTPTimeout(void* arg);
	void onNTPTimeout();

	void processNTP(const ip_addr_t *ipaddr);
    time_t decodeNtpMessage (uint8_t *messageBuffer);
};

extern NTPClient NTP;

#endif // _NtpClientLib_h
