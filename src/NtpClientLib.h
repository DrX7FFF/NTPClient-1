#ifndef _NtpClientLib_h
#define _NtpClientLib_h

#define DEBUG_NTPCLIENT //Uncomment this to enable debug messages over serial port

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

typedef enum NTPSyncEvent {
    timeSyncd = 0, // Time successfully got from NTP server
    noResponse = -1, // No response from server
    invalidAddress = -2, // Address not reachable
    requestSent = 1, // NTP request sent, waiting for response
    errorSending = -3, // An error happened while sending the request
    responseError = -4, // Wrong response received
} NTPSyncEvent_t;

typedef enum NTPStatus {
    syncd = 0, // Time synchronized correctly
    unsyncd = -1, // Time may not be valid
    ntpRequested = 1, // NTP request sent, waiting for response
} NTPStatus_t; // Only for internal library use

typedef enum DNSStatus {
	DNS_IDLE = 0, // Idle state
	DNS_REQUESTED = 1, // DNS resolution requested, waiting for response
    DNS_SOLVED = 2,
} DNSStatus_t; // Only for internal library use//

typedef enum ProcessStatus {
	PROCESS_IDLE = 0,		// Idle state
	PROCESS_DNS = 1,		// DNS resolution requested, waiting for response
	PROCESS_NTP = 2			// NTP resolution requested, waiting for response
} ProcessStatus_t; // Only for internal library use//


#if defined ARDUINO_ARCH_ESP8266 || defined ARDUINO_ARCH_ESP32
#include <functional>
typedef std::function<void (NTPSyncEvent_t)> onSyncEvent_t;
#else
typedef void (*onSyncEvent_t)(NTPSyncEvent_t);
#endif

class NTPClient {
public:
    /**
    * Construct NTP client.
    */
    NTPClient ();

    /**
    * NTP client Class destructor
    */
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

    /**
    * Sets NTP server name.
    * @param[in] New NTP server name.
    * @param[out] True if everything went ok.
    */
    bool setNtpServerName (String ntpServerName);
    bool setNtpServerName (char* ntpServerName);

    /**
    * Sets NTP server name. DEPRECATED, only for compatibility with older versions
    * @param[in] New NTP server name.
    * @param[in] Server index (0-2).
    * @param[out] True if everything went ok.
    */
    bool setNtpServerName (String ntpServerName, int idx) {
        if (idx < 0 || idx > 2)
            return false;
        return setNtpServerName (ntpServerName);
    }

    /**
    * Gets NTP server name
    * @param[out] NTP server name.
    */
    String getNtpServerName ();

    /**
    * Gets NTP server name in char array format
    * @param[out] NTP server name.
    */
    char* getNtpServerNamePtr ();

    /**
    * Gets NTP server name. DEPRECATED, only for compatibility with older versions
    * @param[in] Server index (0-2).
    * @param[out] NTP server name.
    */
    String getNtpServerName (int idx) {
        if (idx < 0 || idx > 2)
            return "";
        return getNtpServerName ();
    }

    /**
    * Starts a NTP time request to server. Returns a time in UNIX time format. Normally only called from library.
    * Kept in public section to allow direct NTP request.
    * @param[out] Time in UNIX time format.
    */
    time_t getTime ();

	void processStart();
    /**
    * Sets timezone.
    * @param[in] New time offset in hours (-11 <= timeZone <= +13).
    * @param[out] True if everything went ok.
    */
    bool setTimeZone (int8_t timeZone, int8_t minutes = 0);

    /**
    * Gets timezone.
    * @param[out] Time offset in hours (plus or minus).
    */
    int8_t getTimeZone ();

    /**
    * Gets minutes fraction of timezone.
    * @param[out] Minutes offset (plus or minus) added to hourly offset.
    */
    int8_t getTimeZoneMinutes ();

    /**
    * Sets DST zone.
    * @param[in] New DST zone (DST_ZONE_EU || DST_ZONE_USA).
    * @param[out] True if everything went ok.
    */
    bool setDSTZone (uint8_t dstZone);

    /**
    * Gets DST zone.
    * @param[out] DST zone.
    */
    uint8_t getDSTZone ();

    /**
    * Stops time synchronization.
    * @param[out] True if everything went ok.
    */
    void stop ();

    /**
    * Changes sync period.
    * @param[in] New interval in seconds.
    * @param[out] True if everything went ok.
    */
    bool setInterval (int interval);

    /**
    * Changes sync period in sync'd and not sync'd status.
    * @param[in] New interval while time is not first adjusted yet, in seconds.
    * @param[in] New interval for normal operation, in seconds.
    * @param[out] True if everything went ok.
    */
    bool setInterval (int shortInterval, int longInterval);

    /**
    * Gets sync period.
    * @param[out] Interval for normal operation, in seconds.
    */
    int getInterval ();

    /**
    * Changes sync period not sync'd status.
    * @param[out] Interval while time is not first adjusted yet, in seconds.
    */
    int	getShortInterval ();

    /**
    * Gets sync period.
    * @param[out] Interval for normal operation in seconds.
    */
    int	getLongInterval () { return getInterval (); }

	void setNextInterval(int interval);
	int getNextInterval();

    /**
    * Set daylight time saving option.
    * @param[in] true is daylight time savings apply.
    */
    void setDayLight (bool daylight);

    /**
    * Get daylight time saving option.
    * @param[out] true is daylight time savings apply.
    */
    bool getDayLight ();

    /**
    * Convert current time to a String.
    * @param[out] String constructed from current time.
    * TODO: Add internationalization support
    */
    String getTimeStr () { return getTimeStr (now ()); }

    /**
    * Convert a time in UNIX format to a String representing time.
    * @param[out] String constructed from current time.
    * @param[in] time_t object to convert to extract time.
    * TODO: Add internationalization support
    */
    String getTimeStr (time_t moment);

    /**
    * Convert current date to a String.
    * @param[out] String constructed from current date.
    * TODO: Add internationalization support
    */
    String getDateStr () { return getDateStr (now ()); }

    /**
    * Convert a time in UNIX format to a String representing its date.
    * @param[out] String constructed from current date.
    * @param[in] time_t object to convert to extract date.
    * TODO: Add internationalization support
    */
    String getDateStr (time_t moment);

    /**
    * Convert current time and date to a String.
    * @param[out] String constructed from current time.
    * TODO: Add internationalization support
    */
    String getTimeDateString () { return getTimeDateString (now ()); }

    /**
    * Convert current time and date to a String.
    * @param[in] time_t object to convert to String.
    * @param[out] String constructed from current time.
    * TODO: Add internationalization support
    */
    String getTimeDateString (time_t moment);

    /**
    * Gets last successful sync time in UNIX format.
    * @param[out] Last successful sync time. 0 equals never.
    */
    time_t getLastNTPSync ();

    /**
    * Get first successful synchronization time after boot.
    * @param[out] First sync time.
    */
    time_t getFirstSync ();

    /**
    * Get configured response timeout for NTP requests.
    * @param[out] NTP Timeout.
    */
    uint16_t getNTPTimeout ();

    /**
    * Configure response timeout for NTP requests.
    * @param[out] error code. false if faulty.
    */
    boolean setNTPTimeout (uint16_t milliseconds);

    /**
    * Set a callback that triggers after a sync trial.
    * @param[in] function with void(NTPSyncEvent_t) or std::function<void(NTPSyncEvent_t)> (only for ESP8266)
    *				NTPSyncEvent_t equals 0 is there is no error
    */
    void onNTPSyncEvent (onSyncEvent_t handler);

    /**
    * True if current time is inside DST period (aka. summer time). False otherwise of if NTP object has DST
    * calculation disabled
    * @param[out] True = summertime enabled and time in summertime period
    *			  False = sumertime disabled or time ouside summertime period
    */
    boolean isSummerTime () {
        if (_daylight)
            return isSummerTimePeriod (now ());
        else
            return false;
    }

    /**
    * True if given time is inside DST period (aka. summer time). False otherwise.
    * @param[in] time to make the calculation with
    * @param[out] True = time in summertime period
    *			  False = time ouside summertime period
    */
    boolean isSummerTimePeriod (time_t moment);

protected:

    AsyncUDP *udp;              ///< UDP connection object
    IPAddress ntpServerIPAddress;
    bool _daylight;             ///< Does this time zone have daylight saving?
    int8_t _timeZone = 0;       ///< Keep track of set time zone offset
    int8_t _minutesOffset = 0;   ///< Minutes offset for time zones with decimal numbers
    uint8_t _dstZone = DEFAULT_DST_ZONE; ///< Daylight save time zone
    char _ntpServerName[SERVER_NAME_LENGTH];       ///< Name of NTP server on Internet or LAN
    int _shortInterval = DEFAULT_NTP_SHORTINTERVAL;         ///< Interval to set periodic time sync until first synchronization.
    int _longInterval = DEFAULT_NTP_INTERVAL;          ///< Interval to set periodic time sync
	int _nextInterval = 0;      ///< Last interval to set periodic time sync
	time_t _lastSyncd = 0;      ///< Stored time of last successful sync
    uint16_t ntpTimeout = 1500; ///< Response timeout for NTP requests
    onSyncEvent_t onSyncEvent;  ///< Event handler callback

    NTPStatus_t status = unsyncd; ///< Sync status
    DNSStatus_t dnsStatus = DNS_IDLE; ///< DNS request status
	ProcessStatus_t processStatus = PROCESS_IDLE; ///<Status du process NTP
    Ticker responseTimer;       ///< Timer to trigger response timeout
    Ticker responseTimer2;       ///< Timer to trigger response timeout
	Ticker timeOutDNS;
	Ticker timeOutNTP;
	Ticker processTimer;

                                /**
                                * Get packet response and update time as of its data
                                * @param[in] UDP response packet.
                                */
    void processPacket (AsyncUDPPacket& packet);

    /**
    * Send NTP request to server
    * @param[in] UDP connection.
    * @param[out] false in case of any error.
    */
    boolean sendNTPpacket (AsyncUDP *udp);

    /**
    * Process internal state in case of a response timeout. If a response comes later is is asumed as non valid.
    */
    void ICACHE_RAM_ATTR processRequestTimeout ();

    /**
    * Static method for Ticker argument.
    */
    static void ICACHE_RAM_ATTR s_processRequestTimeout (void* arg);

    static void s_dnsFound (const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    void dnsFound (const ip_addr_t *ipaddr);
    static void ICACHE_RAM_ATTR s_processDNSTimeout (void* arg);
    void processDNSTimeout ();

	static void s_dnsFound2(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
	void dnsFound2(const ip_addr_t *ipaddr);
	static void ICACHE_RAM_ATTR s_dnsTimeout(void* arg);
	void dnsTimeout();

	void processNTP(const ip_addr_t *ipaddr);

    /**
    * Function that gets time from NTP server and convert it to Unix time format
    * @param[out] Time form NTP in Unix Time Format.
    */
    static time_t s_getTime ();

    /**
    * Calculates the daylight saving for a given date.
    * @param[in] Year.
    * @param[in] Month.
    * @param[in] Day.
    * @param[in] Hour.
    * @param[in] Weekday (1 for sunday).
    * @param[in] Time zone offset.
    * @param[out] true if date and time are inside summertime period.
    */
    bool summertime (int year, byte month, byte day, byte hour, byte weekday, byte tzHours);

    /**
    * Helper function to add leading 0 to hour, minutes or seconds if < 10.
    * @param[in] Digit to evaluate the need of leading 0.
    * @param[out] Result digit with leading 0 if needed.
    */
    //String printDigits(int digits);

	void notifyEvent(NTPSyncEvent_t requestSent);
	
		

public:
    /**
    * Decode NTP response contained in buffer.
    * @param[in] Pointer to message buffer.
    * @param[out] Decoded time from message, 0 if error ocurred.
    */
    time_t decodeNtpMessage (uint8_t *messageBuffer);

private:
    /**
    * Sends NTP request packet to given IP address.
    * @param[in] NTP server's IP address.
    * @param[out] True if everything went ok.
    */
    //bool sendNTPpacket(IPAddress &address);
//#endif
};

extern NTPClient NTP;

#endif // _NtpClientLib_h
