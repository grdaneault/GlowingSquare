
#ifndef GLOWINGSQUARE_TIME_H
#define GLOWINGSQUARE_TIME_H

#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;

// By default 'pool.ntp.org' is used with 60 seconds update interval and
// no offset
NTPClient timeClient(ntpUDP, -4 * 3600);
#endif //GLOWINGSQUARE_TIME_H
