#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// Fill out with your information and rename file to credentials.h

// Change to your wifi ssid and password
char SSID[] = "";
char PASS[] = "";

// Openweather API key
char ONECALLKEY[] = "";

float myLATITUDE = 1.1; // I got this from Wikipedia
float myLONGITUDE = 2.1;

// the time zone to be used
int timeZone = 4; //

// google calendar (or any url that downloads an .ics (iCal) of the calendar)
// go to Calendar/Settings/Private adress in iCal format
char *calendarURL[] = {"url1", "url2", "url3"}; // seperate by comma for multiple
const int num_Calendars = 3;                    // how many of the above to be used

const char *calNames[] = {"cal1", "cal2", "cal3"}; // this will be displayed before the calendar entries in bold, can be empty

#endif
