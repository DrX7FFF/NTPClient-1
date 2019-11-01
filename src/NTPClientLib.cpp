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

bool NTPClient::begin (String ntpServerName, int8_t timeZone, uint8_t DSTZone, int8_t minutes, AsyncUDP* udp_conn) {
    if (!setNtpServerName (ntpServerName)) {
        DEBUGLOG ("Time sync not started\r\n");
        return false;
    }
    if (!setTimeZone (timeZone, minutes)) {
        DEBUGLOG ("Time sync not started\r\n");
        return false;
    }
	if (!setDSTZone(DSTZone)) {
		DEBUGLOG("Time sync not started\r\n");
		return false;
	}

    if (udp_conn)
        udp = udp_conn;
    else if (!udp) // Check if upd connection was already created
        udp = new AsyncUDP ();

    _lastSyncd = 0;

    DEBUGLOG ("Time sync started\r\n");

    return true;
}

void NTPClient::start() {
	DEBUGLOG("%s(%d) ", __FUNCTION__, getShortInterval());
	setNextInterval(getShortInterval());
}

void NTPClient::stop () {
	tickProcess.detach();
	tickTimeout.detach();
	setNextInterval(0);
    if (udp) {
        udp->close ();
        delete (udp);
        udp = 0;
    }
    DEBUGLOG ("Time sync disabled\n");
}

void NTPClient::processDone(NTPStatus_t newStatus) {
	updateStatus(newStatus);
	if (newStatus == syncd)
		setNextInterval(getLongInterval());
	else if (newStatus < 0)
		setNextInterval(getShortInterval());
}

void ICACHE_RAM_ATTR NTPClient::s_onProcessing(void* arg) {
	reinterpret_cast<NTPClient*>(arg)->onProcessing();
}

void NTPClient::onProcessing() {
	tickProcess.detach();
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
		processDone(errorDNS);
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
			processDone(errorSending);
	}
	else
		processDone(errorNoResponse);
}

void NTPClient::s_onDNSFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
	reinterpret_cast<NTPClient*>(callback_arg)->onDNSFound(ipaddr);
}

void NTPClient::onDNSFound(const ip_addr_t *ipaddr) {
	tickTimeout.detach();
	if (!ipaddr)
		processDone(errorInvalidAddress);
	else
		processNTP(ipaddr);
}

void ICACHE_RAM_ATTR NTPClient::s_onDNSTimeout(void* arg) {
	reinterpret_cast<NTPClient*>(arg)->onDNSTimeout();
}

void NTPClient::onDNSTimeout() {
	tickTimeout.detach();
	processDone(errorTimeOutDNS);
}

void ICACHE_RAM_ATTR NTPClient::s_onNTPTimeout(void* arg) {
	reinterpret_cast<NTPClient*>(arg)->onNTPTimeout();
}

void NTPClient::onNTPTimeout() {
	tickTimeout.detach();
	processDone(errorTimeOutNTP);
}

void dumpNTPPacket(byte *data, size_t length) {
	for (size_t i = 0; i < length; i++) {
		DEBUGLOG("%02X ", data[i]);
		if ((i + 1) % 16 == 0)
			DEBUGLOG("\n");
		else if ((i + 1) % 4 == 0)
			DEBUGLOG("| ");
	}
}

boolean NTPClient::sendNTPpacket(AsyncUDP *udp) {
	AsyncUDPMessage ntpPacket = AsyncUDPMessage();

	uint8_t ntpPacketBuffer[NTP_PACKET_SIZE]; //Buffer to store request message
	memset(ntpPacketBuffer, 0, NTP_PACKET_SIZE);
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
			if (timeValue == 0)
				processDone(errorResponse);
			else {
				setTime(timeValue);
				_lastSyncd = timeValue;
				processDone(syncd);
			}
		}
		else
			processDone(errorResponse);
	}
	else {
		DEBUGLOG("Unrequested response\n");
	}
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

void NTPClient::setNextInterval(int interval) {
	_nextInterval = interval;
	setSyncInterval(_nextInterval);
	if (_nextInterval>0)
		tickProcess.once_ms(_nextInterval * 1000, &NTPClient::s_onProcessing, static_cast<void*>(this));
}

int NTPClient::getNextInterval() {
	return _nextInterval;
}

void NTPClient::setInterval (int shortInterval, int longInterval) {
    _shortInterval = shortInterval >= 10 ? shortInterval : 10;
    _longInterval = longInterval >= 10 ? longInterval : 10;
    DEBUGLOG ("Short sync interval set to %d\n", shortInterval);
    DEBUGLOG ("Long sync interval set to %d\n", longInterval);
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

// input parameters: "normal time" for year, month, day, hour, weekday and tzHours (0=UTC, 1=MEZ)
bool NTPClient::isSummerTimePeriod(int year, byte month, byte day, byte hour, byte weekday){
	switch(_dstZone){
	case DST_ZONE_NONE:
		DEBUGLOG("%s No DST Zone\n", __FUNCTION__);
		return false;
	case DST_ZONE_EU:
		DEBUGLOG("%s EU DST Zone\n", __FUNCTION__);
		DEBUGLOG("%s Y%d M%d D%d H%d\n", __FUNCTION__, year, month, day, hour);
		if ((month < 3) || (month > 10)) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
        if ((month > 3) && (month < 10)) return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
        if ((month == 3 && ((hour + 24 * day) >= (1 + _timeZone + 24 * (31 - (5 * year / 4 + 4) % 7)))) || ((month == 10 && (hour + 24 * day) < (1 + _timeZone + 24 * (31 - (5 * year / 4 + 1) % 7)))))
            return true;
        else
            return false;
	case DST_ZONE_USA:
		DEBUGLOG("%s USA DST Zone\n", __FUNCTION__);
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

boolean NTPClient::isSummerTimePeriod (time_t moment) {
	TimeElements tm;
	breakTime(moment, tm);
	return isSummerTimePeriod(tmYearToCalendar(tm.Year), tm.Month, tm.Day, tm.Hour, tm.Wday);
}

uint16_t NTPClient::getNTPTimeout () {
    return ntpTimeout;
}

boolean NTPClient::setNTPTimeout (uint16_t milliseconds) {
	ntpTimeout = milliseconds < MIN_NTP_TIMEOUT ? MIN_NTP_TIMEOUT : milliseconds;
	return (milliseconds < MIN_NTP_TIMEOUT);
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
    if (isSummerTimePeriod(timeTemp)){
		DEBUGLOG("%s change to SummerPeriod\n", __FUNCTION__);
        timeTemp += SECS_PER_HOUR;
	}
	else
		DEBUGLOG("%s unchange SummerPeriod\n", __FUNCTION__);

    return timeTemp;
}

void NTPClient::updateStatus(NTPStatus_t newStatus) {
	if (status == newStatus)
		return;

	status = newStatus;
	DEBUGLOG(status == requestNTP?"Request NTP pending" : "");
	DEBUGLOG(status == requestDNS ? "Request DNS pending" : "");
	DEBUGLOG(status == syncd ? "Time synchronized correctly" : "");
	DEBUGLOG(status == unsyncd ? "Time not synchronized" : "");
	DEBUGLOG(status == errorDNS ? "Error DNS unreachable" : "");
	DEBUGLOG(status == errorInvalidAddress ? "Error Address unreachable" : "");
	DEBUGLOG(status == errorTimeOutDNS ? "Error DNS TimeOut" : "");
	DEBUGLOG(status == errorNoResponse ? "Error No response from server" : "");
	DEBUGLOG(status == errorSending ? "Error happened while sending the request" : "");
	DEBUGLOG(status == errorResponse ? "Error Wrong response received" : "");
	DEBUGLOG(status == errorTimeOutNTP ? "Error NTP TimeOut" : "");
	DEBUGLOG("\r\n");
	if (onSyncEvent)
		onSyncEvent(status);
}
NTPClient NTP;
