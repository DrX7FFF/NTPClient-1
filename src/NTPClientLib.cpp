// https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html

#include "NtpClientLib.h"

#define DBG_PORT Serial

#ifdef DEBUG_NTPCLIENT
#define DEBUGLOG(...) DBG_PORT.printf(__VA_ARGS__)
#else
#define DEBUGLOG(...)
#endif

NTPClient::NTPClient () {
}

NTPClient::~NTPClient () {
    stop ();
}

bool NTPClient::begin (String ntpServerName, int8_t timeZone, bool daylight, int8_t minutes, AsyncUDP* udp_conn) {
    if (!setNtpServerName (ntpServerName)) {
        DEBUGLOG ("Time sync not started\r\n");
        return false;
    }
    if (!setTimeZone (timeZone, minutes)) {
        DEBUGLOG ("Time sync not started\r\n");
        return false;
    }
    if (udp_conn)
        udp = udp_conn;
    else if (!udp) // Check if upd connection was already created
        udp = new AsyncUDP ();

    //_timeZone = timeZone;
    setDayLight (daylight);
    _lastSyncd = 0;

    if (_shortInterval == 0 && _longInterval == 0) {
        if (!setInterval (DEFAULT_NTP_SHORTINTERVAL, DEFAULT_NTP_INTERVAL)) {
            DEBUGLOG ("Time sync not started\r\n");
            return false;
        }
    }
    DEBUGLOG ("Time sync started\r\n");

	setNextInterval(getShortInterval ());
//    setSyncProvider (s_getTime);

    return true;
}

void NTPClient::stop () {
//    setSyncProvider (NULL);
	tickTimeout.detach();
	//updateStatus(errorTimeOutDNS); //Status d'arrêt
    // Free up connection resources
    if (udp) {
        udp->close ();
        delete (udp);
        udp = 0;
    }
    DEBUGLOG ("Time sync disabled\n");
}

bool NTPClient::setNtpServerName (String ntpServerName) {
    uint8_t strLen = ntpServerName.length ();
    if (strLen > SERVER_NAME_LENGTH || strLen <= 0)
        return false;
    ntpServerName.toCharArray (_ntpServerName, SERVER_NAME_LENGTH);
    return true;
}

bool NTPClient::setNtpServerName (char* ntpServerName) {
    char *name = ntpServerName;
    if (!name)
        return false;
    if (!strlen (name))
        return false;
    memset (_ntpServerName, 0, SERVER_NAME_LENGTH);
    strcpy (_ntpServerName, name);
    return true;
}

String NTPClient::getNtpServerName () {
    return String (_ntpServerName);
}

char* NTPClient::getNtpServerNamePtr () {
    return _ntpServerName;
}

bool NTPClient::setTimeZone (int8_t timeZone, int8_t minutes) {
    if ((timeZone >= -12) && (timeZone <= 14) && (minutes >= -59) && (minutes <= 59)) {
        if (_lastSyncd > 0) {
            int8_t timeDiff = timeZone - _timeZone;
            int8_t minDiff = minutes - _minutesOffset;
            setTime (now () + timeDiff * SECS_PER_HOUR + minDiff * SECS_PER_MIN);
        }
        _timeZone = timeZone;
        _minutesOffset = minutes;
        return true;
    }
    return false;
}

int8_t NTPClient::getTimeZone () {
    return _timeZone;
}

int8_t NTPClient::getTimeZoneMinutes () {
    return _minutesOffset;
}


bool NTPClient::setDSTZone (uint8_t dstZone) {
    if (dstZone >= DST_ZONE_COUNT)
		return false;
    _dstZone = dstZone;
    return true;
}

uint8_t NTPClient::getDSTZone () {
    return _dstZone;
}

void NTPClient::s_onDNSFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
	reinterpret_cast<NTPClient*>(callback_arg)->onDNSFound(ipaddr);
}

void NTPClient::onDNSFound(const ip_addr_t *ipaddr) {
	tickTimeout.detach();
	if (!ipaddr)
		updateStatus(errorInvalidAddress);
	else
		processNTP(ipaddr);
}

void ICACHE_RAM_ATTR NTPClient::s_onDNSTimeout(void* arg) {
	reinterpret_cast<NTPClient*>(arg)->onDNSTimeout();
}

void NTPClient::onDNSTimeout() {
	tickTimeout.detach();
	updateStatus(errorTimeOutDNS);
}

void ICACHE_RAM_ATTR NTPClient::s_onNTPTimeout(void* arg) {
	reinterpret_cast<NTPClient*>(arg)->onNTPTimeout();
}

void NTPClient::onNTPTimeout() {
	tickTimeout.detach();
	updateStatus(errorTimeOutNTP);
}

void NTPClient::processStart() {
	if (status >0) {
		DEBUGLOG("%s - Busy ...\n", __FUNCTION__);
		return;
	}
	uint16_t dnsTimeout = 5000;

	updateStatus(requestDNS);
	ip_addr_t ipaddr;
	err_t error = dns_gethostbyname(getNtpServerName().c_str(), &ipaddr, (dns_found_callback)&s_onDNSFound, this);
	switch (error) {
	case ERR_OK:
		processNTP(&ipaddr);
		break;
	case ERR_INPROGRESS:
		// TimeOut de 5000 ms
		tickTimeout.once_ms(dnsTimeout, &NTPClient::s_onDNSTimeout, static_cast<void*>(this));
		break;
	default:
		updateStatus(errorDNS);
		DEBUGLOG("%s - Error abort code : %d\n", __FUNCTION__, error);
	}
}

void NTPClient::processNTP(const ip_addr_t *ipaddr) {
	if (udp->connect(ipaddr, DEFAULT_NTP_PORT)) {
		udp->onPacket(std::bind(&NTPClient::packetReceive, this, _1));
		if (sendNTPpacket(udp)) {
			DEBUGLOG("%s - Send NTP Query to %s\n", __FUNCTION__, IPAddress(ipaddr).toString().c_str());
			updateStatus(requestNTP);
			tickTimeout.once_ms(ntpTimeout, &NTPClient::s_onNTPTimeout, static_cast<void*>(this));
		}
		else
			updateStatus(errorSending);
	}
	else
		updateStatus(errorNoResponse);
}

void dumpNTPPacket (byte *data, size_t length) {
    //byte *data = packet.data ();
    //size_t length = packet.length ();

    for (size_t i = 0; i < length; i++) {
        DEBUGLOG ("%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            DEBUGLOG ("\n");
        } else if ((i + 1) % 4 == 0) {
            DEBUGLOG ("| ");
        }
    }
}

boolean NTPClient::sendNTPpacket (AsyncUDP *udp) {
    AsyncUDPMessage ntpPacket = AsyncUDPMessage ();

    uint8_t ntpPacketBuffer[NTP_PACKET_SIZE]; //Buffer to store request message
    memset (ntpPacketBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    ntpPacketBuffer[0] = 0b11100011;   // LI, Version, Mode
    ntpPacketBuffer[1] = 0;     // Stratum, or type of clock
    ntpPacketBuffer[2] = 6;     // Polling Interval
    ntpPacketBuffer[3] = 0xEC;  // Peer Clock Precision
                                // 8 bytes of zero for Root Delay & Root Dispersion
    ntpPacketBuffer[12] = 49;
    ntpPacketBuffer[13] = 0x4E;
    ntpPacketBuffer[14] = 49;
    ntpPacketBuffer[15] = 52;

	// all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
	ntpPacket.write(ntpPacketBuffer, NTP_PACKET_SIZE);
//	dumpNTPPacket(ntpPacket.data(), ntpPacket.length());
	return udp->send(ntpPacket);
}

void NTPClient::packetReceive(AsyncUDPPacket& packet) {
	tickTimeout.detach();

	DEBUGLOG("UDP Packet Type: %s, From: %s:%d, To: %s:%d, Length: %u, Data:\n",
		packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast",
		packet.remoteIP().toString().c_str(),
		packet.remotePort(),
		packet.localIP().toString().c_str(),
		packet.localPort(),
		packet.length());
	//reply to the client
	dumpNTPPacket(packet.data(), packet.length());

	if (status == requestNTP) {
		if (packet.length() >= NTP_PACKET_SIZE) {
			time_t timeValue = decodeNtpMessage(packet.data());
			if (timeValue == 0) {
				DEBUGLOG("Response 0 Error\n");
				updateStatus(errorResponse);
			}
			else {
				//			setTime(timeValue);
				_lastSyncd = timeValue;
				DEBUGLOG("Successful NTP sync at %d\n", timeValue);
				updateStatus(syncd);
			}
		}
		else
			updateStatus(errorResponse);
	}
	else {
		DEBUGLOG("Unrequested response\n");
	}
}

void NTPClient::setNextInterval(int interval) {
	_nextInterval = interval;
	setSyncInterval(_nextInterval);
}

int NTPClient::getNextInterval() {
	return _nextInterval;
}

bool NTPClient::setInterval (int interval) {
    if (interval >= 10) {
        if (_longInterval != interval) {
            _longInterval = interval;
            DEBUGLOG ("Sync interval set to %d\n", interval);
            if (timeStatus () == timeSet)
				setNextInterval(interval);
        }
        return true;
    } else
        return false;
}

bool NTPClient::setInterval (int shortInterval, int longInterval) {
    if (shortInterval >= 10 && longInterval >= 10) {
        _shortInterval = shortInterval;
        _longInterval = longInterval;
        if (timeStatus () != timeSet) {
			setNextInterval(shortInterval);
        } else {
			setNextInterval(longInterval);
        }
        DEBUGLOG ("Short sync interval set to %d\n", shortInterval);
        DEBUGLOG ("Long sync interval set to %d\n", longInterval);
        return true;
    } else
        return false;
}

int NTPClient::getInterval () {
    return _longInterval;
}

int NTPClient::getShortInterval () {
    return _shortInterval;
}

void NTPClient::setDayLight (bool daylight) {

    // Do the maths to change current time, but only if we are not yet sync'ed,
    // we don't want to trigger the UDP query with the now() below
    if (_lastSyncd > 0) {
        if ((_daylight != daylight) && isSummerTimePeriod (now ())) {
            if (daylight) {
                setTime (now () + SECS_PER_HOUR);
            } else {
                setTime (now () - SECS_PER_HOUR);
            }
        }
    }

    _daylight = daylight;
    DEBUGLOG ("--Set daylight saving %s\n", daylight ? "ON" : "OFF");

}

bool NTPClient::getDayLight () {
    return _daylight;
}

String NTPClient::getTimeStr (time_t moment) {
    char timeStr[10];
    sprintf (timeStr, "%02d:%02d:%02d", hour (moment), minute (moment), second (moment));

    return timeStr;
}

String NTPClient::getDateStr (time_t moment) {
    char dateStr[12];
    sprintf (dateStr, "%02d/%02d/%4d", day (moment), month (moment), year (moment));

    return dateStr;
}

String NTPClient::getTimeDateString (time_t moment) {
    return getTimeStr (moment) + " " + getDateStr (moment);
}

time_t NTPClient::getLastNTPSync () {
    return _lastSyncd;
}

void NTPClient::onNTPSyncEvent(onSyncEvent_t handler) {
	onSyncEvent = handler;
}

bool NTPClient::summertime (int year, byte month, byte day, byte hour, byte weekday, byte tzHours)
// input parameters: "normal time" for year, month, day, hour, weekday and tzHours (0=UTC, 1=MEZ)
{
    if (DST_ZONE_EU == _dstZone) {
        if ((month < 3) || (month > 10)) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
        if ((month > 3) && (month < 10)) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
        if ((month == 3 && ((hour + 24 * day) >= (1 + tzHours + 24 * (31 - (5 * year / 4 + 4) % 7)))) || ((month == 10 && (hour + 24 * day) < (1 + tzHours + 24 * (31 - (5 * year / 4 + 1) % 7)))))
            return true;
        else
            return false;
    }

    if (DST_ZONE_USA == _dstZone) {

        // always false for Jan, Feb and Dec
        if ((month < 3) || (month > 11)) return false;

        // always true from Apr to Oct
        if ((month > 3) && (month < 11)) return true;

        // first sunday of current month
        uint8_t first_sunday = (7 + day - weekday) % 7 + 1;

        // Starts at 2:00 am on the second sunday of Mar
        if (3 == month) {
            if (day < 7 + first_sunday) return false;
            if (day > 7 + first_sunday) return true;
            return (hour > 2);
        }

        // Ends a 2:00 am on the first sunday of Nov
        // We are only getting here if its Nov
        if (day < first_sunday) return true;
        if (day > first_sunday) return false;
        return (hour < 2);

    }
    return false;
}

<<<<<<< HEAD
boolean NTPClient::isSummerTimePeriod (time_t moment) {
=======
boolean NTPClient::isSummerTimePeriod (time_t moment) { 
>>>>>>> bcae44eaf088bb6594c14ff7518e74977e57db96
    return summertime (year (moment), month (moment), day (moment), hour (moment), weekday (moment), getTimeZone ());
}

uint16_t NTPClient::getNTPTimeout () {
    return ntpTimeout;
}

boolean NTPClient::setNTPTimeout (uint16_t milliseconds) {
    if (milliseconds < MIN_NTP_TIMEOUT)
		return false;
	ntpTimeout = milliseconds;
	return true;
}

time_t NTPClient::decodeNtpMessage (uint8_t *messageBuffer) {
    unsigned long secsSince1900;
    // convert four bytes starting at location 40 to a long integer
    secsSince1900 = (unsigned long)messageBuffer[40] << 24;
    secsSince1900 |= (unsigned long)messageBuffer[41] << 16;
    secsSince1900 |= (unsigned long)messageBuffer[42] << 8;
    secsSince1900 |= (unsigned long)messageBuffer[43];

    if (secsSince1900 == 0)
        return 0;

#define SEVENTY_YEARS 2208988800UL
    time_t timeTemp = secsSince1900 - SEVENTY_YEARS + _timeZone * SECS_PER_HOUR + _minutesOffset * SECS_PER_MIN;

    if (_daylight)
        if (summertime (year (timeTemp), month (timeTemp), day (timeTemp), hour (timeTemp), weekday (timeTemp), _timeZone))
            timeTemp += SECS_PER_HOUR;

    return timeTemp;
}

void NTPClient::updateStatus(NTPStatus_t newstatus) {
	status = newstatus;
	DEBUGLOG(getStatusString().c_str());
	DEBUGLOG("\r\n");
	if (onSyncEvent)
		onSyncEvent(newstatus);
}

String NTPClient::getStatusString() {
	switch (status) {
	case requestNTP:
		return "Request NTP pending";
	case requestDNS :
		return "Request DNS pending";
	case syncd:
		return "Time synchronized correctly";
	case unsyncd:
		return "Time not synchronized";
	case errorDNS:
		return "Error DNS unreachable";
	case errorInvalidAddress:
		return "Error Address unreachable";
	case errorTimeOutDNS:
		return "Error DNS TimeOut";
	case errorNoResponse:
		return "Error No response from server";
	case errorSending:
		return "Error happened while sending the request";
	case errorResponse:
		return "Error Wrong response received";
	case errorTimeOutNTP:
		return "Error NTP TimeOut";
	default:
		return "Error unknown";
	}
}
NTPClient NTP;
