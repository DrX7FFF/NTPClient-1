/*
 Name:		NtpClientLib
 Created:	17/08/2016
 Author:	Germán Martín (gmag11@gmail.com)
 Maintainer:Germán Martín (gmag11@gmail.com)
 Editor:	http://www.visualmicro.com

 Library to get system sync from a NTP server
*/

#ifndef _NtpClientLib_h
#define _NtpClientLib_h

#define DEBUG_NTPCLIENT //Uncomment this to enable debug messages over serial port

#ifdef ESP8266
extern "C" {
#include "user_interface.h"
#include "sntp.h"
}
#include <functional>
using namespace std;
using namespace placeholders;
#endif

#include <TimeLib.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#define NETWORK_W5100           (1)
#define NETWORK_ENC28J60        (2)
#define NETWORK_WIFI101			(3)
#define NETWORK_ESP8266			(100)

#define DEFAULT_NTP_SERVER "pool.ntp.org" // Default international NTP server. I recommend you to select a closer server to get better accuracy
#define DEFAULT_NTP_PORT 123 // Default local udp port. Select a different one if neccesary (usually not needed)
#define DEFAULT_NTP_INTERVAL 1800 // Default sync interval 30 minutes 
#define DEFAULT_NTP_SHORTINTERVAL 15 // Sync interval when sync has not been achieved. 15 seconds
#define DEFAULT_NTP_TIMEZONE 0 // Select your local time offset. 0 if UTC time has to be used


#ifdef ARDUINO_ARCH_ESP8266
#define NETWORK_TYPE NETWORK_ESP8266

#elif defined ARDUINO_ARCH_AVR
#define NETWORK_TYPE NETWORK_W5100
#include <SPI.h>
#include <EthernetUdp.h>
#include <Ethernet.h>
#include <Dns.h>
#include <Dhcp.h>

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message

#else
#error "Incorrect platform. Only ARDUINO and ESP8266 MCUs are valid."
#endif

class NTPClient{
public:
	/**
	* Construct NTP client.
	*/
	NTPClient();

	/**
	* Starts time synchronization.
	* @param[in] NTP server name as String.
	* @param[in] Time offset from UTC.
	* @param[in] true if this time zone has dayligth saving.
	* @param[out] true if everything went ok.
	*/
	bool begin(String ntpServerName = DEFAULT_NTP_SERVER, int timeOffset = DEFAULT_NTP_TIMEZONE, bool daylight = false);

#ifdef ARDUINO_ARCH_ESP8266
	/**
	* Sets NTP server name.
	* @param[in] New NTP server name.
	* @param[in] Server index (0-2).
	* @param[out] True if everything went ok.
	*/
	bool setNtpServerName(String ntpServerName, int idx = 0);

	/**
	* Gets NTP server name
	* @param[in] Server index (0-2).
	* @param[out] NTP server name.
	*/
	String getNtpServerName(int idx = 0);
#endif
#ifdef ARDUINO_ARCH_AVR
	/**
	* Sets NTP server name.
	* @param[in] New NTP server name.
	* @param[out] True if everything went ok.
	*/
	bool	setNtpServerName(String ntpServerName);

	/**
	* Gets NTP server name
	* @param[out] NTP server name.
	*/
	String getNtpServerName();
#endif
	
	/**
	* Sets timezone.
	* @param[in] New time offset in hours (-11 <= timeZone <= +13).
	* @param[out] True if everything went ok.
	*/
	bool setTimeZone(int timeZone);

	/**
	* Gets timezone.
	* @param[out] Time offset in hours (plus or minus).
	*/
	int getTimeZone();

	/**
	* Starts a NTP time request to server. Returns a time in UNIX time format. Normally only called from library.
	* Kept in public section to allow direct NTP request.
	* @param[out] Time in UNIX time format.
	*/
	time_t getTime();

	/**
	* Stops time synchronization.
	* @param[out] True if everything went ok.
	*/
	bool stop();

	/**
	* Changes sync period.
	* @param[in] New interval in seconds.
	* @param[out] True if everything went ok.
	*/
	bool setInterval(int interval);

	/**
	* Changes sync period in sync'd and not sync'd status.
	* @param[in] New interval while time is not first adjusted yet, in seconds.
	* @param[in] New interval for normal operation, in seconds.
	* @param[out] True if everything went ok.
	*/
	bool setInterval(int shortInterval, int longInterval);

	/**
	* Gets sync period.
	* @param[out] Interval for normal operation, in seconds.
	*/
	int getInterval();

	/**
	* Changes sync period not sync'd status.
	* @param[out] Interval while time is not first adjusted yet, in seconds.
	*/
	int	getShortInterval();

	/**
	* Gets sync period.
	* @param[out] Interval for normal operation in seconds.
	*/
	int	getLongInterval() { return getInterval(); }

	/**
	* Set daylight time saving option.
	* @param[in] true is daylight time savings apply.
	*/
	void setDayLight(bool daylight);

	/**
	* Get daylight time saving option.
	* @param[out] true is daylight time savings apply.
	*/
	bool getDayLight();

	/**
	* Convert current time to a String.
	* @param[out] String constructed from current time.
	* TODO: Add internationalization support
	*/
	String getTimeStr();

	/**
	* Convert a time in UNIX format to a String representing time.
	* @param[out] String constructed from current time.
	* @param[in] time_t object to convert to extract time.
	* TODO: Add internationalization support
	*/
	String getTimeStr(time_t moment);

	/**
	* Convert current date to a String.
	* @param[out] String constructed from current date.
	* TODO: Add internationalization support
	*/
	String getDateStr();

	/**
	* Convert a time in UNIX format to a String representing its date.
	* @param[out] String constructed from current date.
	* @param[in] time_t object to convert to extract date.
	* TODO: Add internationalization support
	*/
	String getDateStr(time_t moment);

	/**
	* Convert current time and date to a String.
	* @param[out] String constructed from current time.
	* TODO: Add internationalization support
	*/
	String getTimeDateString();

	/**
	* Convert current time and date to a String.
	* @param[in] time_t object to convert to String.
	* @param[out] String constructed from current time.
	* TODO: Add internationalization support
	*/
	String getTimeDateString(time_t moment);

	/**
	* Gets last successful sync time in UNIX format.
	* @param[out] Last successful sync time. 0 equals never.
	*/
	time_t getLastNTPSync();

	/**
	* Set last successful synchronization time.
	* @param[out] Last sync time.
	*/
	//void setLastNTPSync(time_t moment);

	/**
	* Get uptime in human readable String format.
	* @param[out] Uptime.
	*/
	String getUptimeString();

	/**
	* Get uptime in UNIX format, time since MCU was last rebooted.
	* @param[out] Uptime. 0 equals never.
	*/
	time_t getUptime();

	/**
	* Get first successful synchronization time after boot.
	* @param[out] First sync time.
	*/
	time_t getFirstSync();

protected:

	bool _daylight; //Does this time zone have daylight saving?
	int _shortInterval; //Interval to set periodic time sync until first synchronization.
	int _longInterval; //Interval to set periodic time sync
	time_t _lastSyncd = 0; //Stored time of last successful sync
	time_t _firstSync = 0; //Stored time of first successful sync after boot
	unsigned long _uptime = 0; // Time since boot

	/**
	* Calculates the daylight saving for a given date.
	* @param[in] Year.
	* @param[in] Month.
	* @param[in] Day.
	* @param[in] Hour.
	* @param[in] Time zone offset.
	* @param[out] true if date and time are inside summertime period.
	*/
	bool summertime(int year, byte month, byte day, byte hour, byte tzHours);

	/**
	* Helper function to add leading 0 to hour, minutes or seconds if < 10.
	* @param[in] Digit to evaluate the need of leading 0.
	* @param[out] Result digit with leading 0 if needed.
	*/
	String printDigits(int digits);


#ifdef ARDUINO_ARCH_AVR
private:
	/**
	* Decode NTP response contained in buffer.
	* @param[in] Pointer to message buffer.
	* @param[out] Decoded time from message, 0 if error ocurred.
	*/
	time_t decodeNtpMessage(char *messageBuffer);

	/**
	* Sends NTP request packet to given IP address.
	* @param[in] NTP server's IP address.
	* @param[out] True if everything went ok.
	*/
	bool sendNTPpacket(IPAddress &address);
#endif
};

extern NTPClient NTP;

#endif // _NtpClientLib_h

