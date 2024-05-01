#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// Fill out with your information and rename file to credentials.h

// Change to your wifi ssid and password
#define SSID ""
#define PASS ""

// Openweather API key (with OneCall Subscription!)
char *APIKEY = "";

// the location to be used
float myLatitude = 40.39;
float myLongitude = 10.49;

// the time zone to be used
int timeZone = 2; // use setTimezone instead (?)

// google calendar (or any url that downloads an .ics (iCal) of the calendar)
// go to Calendar/Settings/Private adress in iCal format
char *calendarURL[] = {""};  // seperate by comma for multiple
const int num_Calendars = 3; // how many of the above to be used

const char *calNames[] = {"cal1", "cal2", "cal3"}; // this will be displayed before the calendar entries in bold, can be empty

#endif
