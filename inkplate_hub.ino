/*
    Inkplate Hub by dogerber
    https://github.com/dogerber/inkplate_hub

    Displays weather and appointments for the current and three next days.

    Based on examples by Soldered.
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#if !defined(ARDUINO_INKPLATE10) && !defined(ARDUINO_INKPLATE10V2)
#error \
    "Wrong board selection for this example, please select e-radionica Inkplate10 or Soldered Inkplate10 in the boards menu."
#endif

// WiFi Connection required
#include <WiFi.h>

// Required libraries
#include "HTTPClient.h"
#include <ArduinoJson.h>        // v7.0.1
#include <TimeLib.h>            // v1.6.1
#include <OpenWeatherOneCall.h> // v4.0.3
#include "Inkplate.h"           // v10.0.0

#include "Network.h"
#include <Arduino.h>
#include <algorithm>
#include <ctime>

#include "credentials.h"

// icons
#include "icons.h"

// Including fonts used (adafruit gfx fonts)
#include "Fonts/FreeSans24pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSans18pt7b.h"
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSansBold12pt7b.h"
#include "Fonts/Roboto_Light_120.h"
#include "Fonts/Roboto_Light_36.h"
#include "Fonts/Roboto_Light_48.h"
#include "Fonts/moon_Phases20pt7b.h"
#include "Fonts/moon_Phases36pt7b.h"
#include "Fonts/FreeSerif12pt7b.h"

// ----- Settings -----

// Set to 3 to flip the screen 180 degrees
#define ROTATION 4

// Delay between API calls (between inkplate is sleeping)
#define DELAY_MS 3600e6 // 1h in microseconds

// metric or imperial
bool metric = true; // TRUE is METRIC, FALSE is IMPERIAL, BLANK is KELVIN

// Layout - fix points
// Inkplate has 825 x 1200 pixels
const int layout_XForecast = 540;                                 //  (horizontal) width of the today panel on the left side
const int layout_XForecastCalendarStart = layout_XForecast + 200; // (horizontal) width of each day in the coming days

const int layout_YForecastTopOffset = 40;                                          // (vertical) distance to top for the forecasts
const int layout_YForecastHeight = (E_INK_HEIGHT - layout_YForecastTopOffset) / 3; // size of each of the days in the forecast panel
const int layout_YDateBoxHeight = 100;                                             // (horizontal) size of box that shows todays date
const int layout_YTodayWeatherIcons = 150;                                         // (vertical) where the weather (and moon) for today starts
const int layout_YWeather = 150;                                                   // (vertical) distance from top to weather (numbers)
const int layout_YHourly = 400;                                                    // (vertical) distance from top to hourly forecast
const int layout_YCalendar = 450;                                                  // (vertical) where the calendar starts

// Layout - distances
const int layout_LineHeight = 20; // for normal font
const int layout_Margin = 10;

// Colors
const int color_Lines = 3;
const int color_textNormal = 1;
const int color_textImportant = 0;
const int color_textUnNormal = 4;
const int color_images = 3;

// ----- OpenWeather Settings -----
// char ONECALLKEY[] = """"; see credentials.h

// LOCATION

int myTimeZone = 2; //<--------- GMT OFFSET

// Location MODES are used to tell the program how you are delivering your coordinates:
//  1 LAT/LON  <----- Can be from GPS, manual as below, or whatever method gets you a latitude and longitude except IP location
//  2 CITYID   <----- From OpenWeatherMap City ID list. Not very accurate.
//  3 IP ADDRESS [NOT REALLY SUGGESTED CAN BE WILDLY OFFSET LOCATION IF HOTSPOT]

int locationMode = 1;

// For Latitude and Longitude Location MODE 1 setting if used
// float myLATITUDE = myLatitude; //<-----This is Toms River, NJ, see credentials.h
// float myLONGITUDE = myLongitude;

// // For City ID Location MODE setting if used
int myCITYID = 6295513; //<-----Toms River, NJ USA

// END LOCATION

// FLAGS
int myCURRENT = 1;                     //<-----0 is CURRENT off, 1 is on
int myAIRQUALITY = 0;                  //<-----0 is Air Quality off, 1 is on
int myTIMESTAMP = 0;                   //<-----0 is TIMESTAMP off, 1 is TIMESTAMP on
char timestampDate[11] = "03/03/2023"; //<-----MM/DD/YYYY right here limit is 4 days in future, past is 01/01/1979
int myOVERVIEW = 0;                    //<-----0 is OVERVIEW off, 1 is OVERVIEW on
char overviewDate[9] = "TOMORROW";     //<-----"TODAY" or "TOMORROW"
//***

//** EXCLUDES
// Excludes are EXCL_D, EXCL_H, EXCL_M, EXCL_A
// Those are DAILY, HOURLY, MINUTELY, and ALERTS
// You set them like this: myEXCLUDES = EXCL_D+EXCL_H+EXCL_M+EXCL_A
// If you leave them set to 0 as below you get a JSON file with ALL CURRENT WEATHER measurements which is a huge file

int myEXCLUDES = EXCL_M + EXCL_A; //<-----0 Excludes is default
//*

// UNITS OF MEASURMENT and DATE TIME FORMAT

int myUNITS = METRIC; //<-----METRIC, IMPERIAL, KELVIN (IMPERIAL is default)

// Date Time Format
int myDTF = 1; /*
                 1 M/D/Y 24H
                 2 D/M/Y 24H
                 3 M/D/Y 12H
                 4 D/M/Y 12H
                 5/6 TIME ONLY 24H
                 7/8 TIME ONLY 12H
                 9 DAY SHORTNAME
                 10 M/D/Y ONLY,
                 11 D/M/Y ONLY

                 ISO8601 options:
                 12 YYYY-MM-DD ONLY,
                 13 THH:MM:SS ONLY
                 14 YYYY-MM-DDTHH:MM:SSY 24H
                 */
//***

// ----- initiate variables -----

// Inkplate object
Inkplate display(INKPLATE_3BIT); // was 1 bit for weather example
Network network;
OpenWeatherOneCall OWOC; // Invoke OpenWeather Library
time_t t = now();

// ----- Calendar Variables

// Variables for time and raw event info
char date[64];
char *data;

// Struct for storing calender event info
struct entry
{
    char name[128];
    char time[128];
    char location[128];
    int day = -1;
    int timeStamp;
    double differenceToNowS;
    struct tm time_element; // will have infos like day, weekday etc.
    struct tm time_element_end;
    int calID;
    bool isFullDay;
};

// Define an array of weekday names
const char *weekdays[] = {"Sunntig", "Maentig", "Zistig", "Mittwuch", "Dunnstig", "Fritig", "Samstig"};

struct tm time1, time2, time3, time4;
struct tm dates_to_do[4];

// Here we store calendar entries
int entriesNum = 0;
const int size_entriesNum = 64;
entry entries[size_entriesNum + 1];

// ----- Weather Variables
enum alignment
{
    LEFT_BOT,
    CENTRE_BOT,
    RIGHT_BOT,
    LEFT,
    CENTRE,
    RIGHT,
    LEFT_TOP,
    CENTRE_TOP,
    RIGHT_TOP
};

const char *moonphasenames[29] = {
    "New Moon", "Waxing Crescent", "Waxing Crescent", "Waxing Crescent", "Waxing Crescent", "Waxing Crescent",
    "Waxing Crescent", "Quarter", "Waxing Gibbous", "Waxing Gibbous", "Waxing Gibbous", "Waxing Gibbous",
    "Waxing Gibbous", "Waxing Gibbous", "Full Moon", "Waning Gibbous", "Waning Gibbous", "Waning Gibbous",
    "Waning Gibbous", "Waning Gibbous", "Waning Gibbous", "Last Quarter", "Waning Crescent", "Waning Crescent",
    "Waning Crescent", "Waning Crescent", "Waning Crescent", "Waning Crescent", "Waning Crescent"};

// Contants used for drawing icons
const char abbrs[32][32] = {"01d", "02d", "03d", "04d", "09d", "10d", "11d", "13d", "50d",
                            "01n", "02n", "03n", "04n", "09n", "10n", "11n", "13n", "50n"};
const uint8_t *logos[18] = {
    icon_01d,
    icon_02d,
    icon_03d,
    icon_04d,
    icon_09d,
    icon_10d,
    icon_11d,
    icon_13d,
    icon_50d,
    icon_01n,
    icon_02n,
    icon_03n,
    icon_04n,
    icon_09n,
    icon_10n,
    icon_11n,
    icon_13n,
    icon_50n,
};

// functions defined below
void setTimezone(String timezone);
bool checkIfAPIKeyIsValid(char *ONECALLKEY);
void alignText(const char align, const char *text, int16_t x, int16_t y);
void drawTime();
void drawForecast();
void drawCurrent();
void drawHourly();
void drawMoon();
float getMoonPhase(time_t tdate);
void drawWeather(char WeatherAbbr[], int x, int y);

void getToFrom(char *dst, char *from, char *to, int *day, int *timeStamp, double *differenceToNowS, struct tm *time_element, struct tm *time_element_end);
void parseiCal(int cal_i);
void drawCalendarforDate(struct tm targetDate, int x_start, int y_start);
void drawCalendarforForecastDays();
bool isDateInRange(struct tm checkDate, struct tm startDate, struct tm endDate);
bool isSameDay(const struct tm &time1, const struct tm &time2);
int cmp(const void *a, const void *b);

char Output[200] = {0}; // used for printing to serial

void setTimezone(String timezone)
{
    // see https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
    // and https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
    Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
    setenv("TZ", timezone.c_str(), 1); //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
    tzset();
}

void connectWifi()
{
    int ConnectCount = 20;
    if (WiFi.status() != WL_CONNECTED)
    {
        while (WiFi.status() != WL_CONNECTED)
        {
            if (ConnectCount++ == 20)
            {
                Serial.println("Connect WiFi");
                WiFi.begin(SSID, WLAN_PW);
                Serial.print("Connecting.");
                ConnectCount = 0;
            }
            Serial.print(".");
            delay(1000);
        }
        Serial.print("\nConnected to: ");
        Serial.println(SSID);
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("Connected WiFi");
    }
}

void GetCurrentWeather()
{
    connectWifi();

    Serial.println("Getting weather");
    OWOC.parseWeather(); // API Keys, location etc, are given by the Constants ONECALLKEY, myLATITUDE etc.
    setTime(OWOC.current->dayTime);
    t = now();

    // print weather to serial (for debugging)
    if (true)
    {
        // Location info is available for ALL modes
        printf("\nLocation: % s, % s % s\n", OWOC.location.CITY, OWOC.location.STATE, OWOC.location.COUNTRY);

        // Verify all other values exist before using

        if (OWOC.current) // Check if data is in the struct for all variables before using them
        {
            printf("\nCURRENT\n");
            printf("Temp : % .0f\n", OWOC.current->temperature);
            printf("Humidity : % .0f\n", OWOC.current->humidity);
        }
        else
            printf("\nCURRENT IS OFF or EMPTY\n");

        // Forecast is part of CURRENT and has info for the 8 days future weather. See variables PDF
        //  The JSON holds this info in the DAILY section and EXCL_D turns this off
        if (OWOC.forecast)
        { // Check if data is in the struct for all variables before using them
            printf("\nFORECAST - Up to 8 days future forecast\n");
            for (int x = 0; x < 8; x++)
            {
                printf("Date: %s High Temperature: % .0f\n", OWOC.forecast[x].readableDateTime, OWOC.forecast[x].temperatureHigh);
            }
        }
        else
            printf("\nFORECAST IS OFF or EMPTY\n");

        // Hour is part of CURRENT and has info for 48 hours future weather. See variables PDF
        //  The JSON holds this info in the HOURLY section and EXCL_H turns this off
        if (OWOC.hour)
        { // Check if data is in the struct for all variables before using them
            printf("\nHOURLY   - Up to 48 hours forecast\n");

            for (int x = 0; x < 6; x++)
            {
                printf("%s Actual Temp: % .0f\n", OWOC.hour[x].readableTime, OWOC.hour[x].temperature);
                printf("%s Feels Like Temp: % .0f\n", OWOC.hour[x].readableTime, OWOC.hour[x].apparentTemperature);
            }
        }
        else
            printf("\nHOURLY IS OFF or EMPTY\n");

        // MINUTELY is part of CURRENT and has info for 60 minutes future weather. See variables PDF
        //  The JSON holds this info in the MINUTELY section and EXCL_M turns this off
        if (OWOC.minute)
        { // Check if data is in the struct for all variables before using them
            printf("\nMINUTELY   - Up to 60 minutes precipitation forecast\n");

            for (int x = 0; x < 30; x++)
            {
                printf("%s Precipitation: % .2f\n", OWOC.minute[x].readableTime, OWOC.minute[x].precipitation);
            }
        }
        else
            printf("\nHOURLY IS OFF or EMPTY\n");

        // ALERTS is part of CURRENT and has info for alerts in your area. See variables PDF
        //  The JSON holds this info in the ALERTS section and EXCL_A turns this off
        if (OWOC.alert) // Check if data is in the struct for all variables before using them
        {
            printf("\nALERT *** ALERT *** ALERT\n");
            printf("Sender : % s\n", OWOC.alert->senderName);
            printf("Event : % s\n", OWOC.alert->event);
            printf("ALERT : % s\n", OWOC.alert->summary);
        }
        else
        {
            printf("\nNo Alerts For Area, Alerts are EXCLUDED, or Alerts struct is empty\n");
        }
    }
}

void setup()
{
    // Begin serial and display
    Serial.begin(115200);
    while (!Serial)
    {
        ;
    }
    Serial.println("Serial Monitor Initialized");

    display.begin();
    display.setRotation(ROTATION);
    display.setTextWrap(false);
    display.setTextColor(0, 7); // 0 represents black, 7 represents white and numbers in between represents shades; 1 is lightest grey, 6 represents darkest grey.
    display.clearDisplay();
    // display.display();

    connectWifi();

    // ----- Calendar part -----
    data = (char *)ps_malloc(200000LL); // allocate space for ics download

    network.begin(SSID, WLAN_PW);

    // set timezone
    setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");

    // targeted days
    time_t now; // allocate variable
    time(&now); // sets now to the current time in seconds since 1970
    struct tm *localTime = localtime(&now);
    Serial.println(asctime(localTime));

    if (localTime->tm_isdst)
    {
        Serial.println("Its Daylight saving time.");
        // timeZone = timeZone + 1; // hacky
    }
    else
    {
        Serial.println("Its not Daylight saving time.");
    }

    // Calculate the next four days
    for (int i = 0; i < 4; ++i)
    {
        // Calculate the time for the next day (86400 seconds in a day)
        time_t nextDayTime = now + (i) * 86400;

        // Convert the time_t value to a struct tm representing local time
        struct tm *nextDayStruct = localtime(&nextDayTime);

        // Copy the struct tm to the array
        dates_to_do[i] = *nextDayStruct;
    }

    // ----- download and parse calendar data
    for (int cal_i = 0; cal_i < num_Calendars; cal_i++)
    {
        Serial.println("Starting download calendar ");
        // probably a safety problem Serial.println(calendarURL[cal_i]);

        while (!network.getData(calendarURL[cal_i], data))
        {
            delay(1000);
        }
        // convert iCal data into entries format
        parseiCal(cal_i);
    }

    // ----- drawing step

    // "today"
    int ytd1 = layout_YCalendar;
    display.drawLine(0, ytd1, layout_XForecast, ytd1, color_Lines);
    display.setCursor(layout_XForecast / 2 - 30, ytd1 + layout_LineHeight);
    display.setTextSize(1);
    display.setFont(&FreeSerif12pt7b);
    display.setTextColor(color_textNormal, 7);
    display.print("Heute");
    ytd1 += 35;
    display.drawLine(0, ytd1, layout_XForecast, ytd1, color_Lines);

    // fill calender info for today
    drawCalendarforDate(dates_to_do[0], 50, layout_YCalendar + layout_LineHeight + 40);

    // ----- Weather part -----
    if (true)
    {
        // Check if we have a valid API key:
        Serial.println("Checking if API key is valid...");
        if (!checkIfAPIKeyIsValid(ONECALLKEY))
        {
            // If we don't, notify the user
            Serial.println("API key is invalid!");
            display.clearDisplay();
            display.setCursor(0, 0);
            display.setTextSize(2);
            display.println("Can't get data from OpenWeatherMaps! Check your API key!");
            display.println("Only older API keys for OneCall 2.5 work in free tier.");
            display.println("See the code comments the example for more info.");
            display.display();
            while (1)
            {
                delay(100);
            }
        }
        Serial.println("API key is valid!");

        // Clear display
        // t = now();

        // if ((minute(t) % 30) == 0) // Also returns 0 when time isn't set
        // {
        if (true)
        {
            GetCurrentWeather();
            Serial.println("GetCurrentWeather finished");
            drawForecast();
            Serial.println("drawForecast finished");
            drawCurrent();
            Serial.println("drawCurrent finished");
            drawMoon();
            Serial.println("drawMoon finished!");
            drawHourly();
        }

        // styling
        display.drawThickLine(layout_XForecast, 0, layout_XForecast, E_INK_HEIGHT, 2, 2); // vertical

        for (int li = 0; li < 3; li++)
        {
            int ytd = layout_YForecastTopOffset + layout_YForecastHeight * li + 4;
            display.drawThickLine(layout_XForecast, ytd, layout_XForecast + E_INK_WIDTH, ytd, color_Lines, 1); // horizontal, bottom line
            ytd -= 33;
            display.drawThickLine(layout_XForecast, ytd, layout_XForecast + E_INK_WIDTH, ytd, color_Lines, 1); // horizontal, top line
        }

        // display.drawLine(0, layout_YCalendar, E_INK_WIDTH, layout_YCalendar, BLACK); // horizontal
        // display.drawLine(0, layout_YCalendar+layout_LineHeight, E_INK_WIDTH, layout_YCalendar+layout_LineHeight, BLACK); // horizontal

        // write voltage to bottom right corner
        // float voltage = display.readBattery(); // Read battery voltage
        // display.setCursor(700, 1000);
        // display.setTextColor(color_textNormal,7);
        // display.setFont(&FreeSans9pt7b);
        // display.print(voltage, 2); // Print battery voltage
        // display.print('V');
    }

    // calendar and time information for forecast days
    drawCalendarforForecastDays();
    Serial.println("drawCalendarforForecastDays finished!");
    drawTime();
    Serial.println("drawTime finished!");

    display.display();

    // Go to sleep before checking again
    Serial.println("Going to deepsleep.");
    esp_sleep_enable_timer_wakeup(DELAY_MS);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, LOW); // Enable wakeup from deep sleep on gpio 36 (wake button)
    (void)esp_deep_sleep_start();
}

void loop()
{
    // never reached, because it goes to deep sleep at the end of setup().
}

// Format event times, example 13:00 to 14:00
void getToFrom(char *dst, char *from, char *to, int *day, int *timeStamp, double *differenceToNowS, struct tm *time_element, struct tm *time_element_end)
{
    // ANSI C time struct
    struct tm ltm = {0}, ltm2 = {0};
    char temp[128], temp2[128];
    strncpy(temp, from, 16);
    temp[16] = 0;
    // Serial.println(temp);

    // https://github.com/esp8266/Arduino/issues/5141, quickfix
    memmove(temp + 5, temp + 4, 16);
    memmove(temp + 8, temp + 7, 16);
    memmove(temp + 14, temp + 13, 16);
    memmove(temp + 16, temp + 15, 16);
    temp[4] = temp[7] = temp[13] = temp[16] = '-';

    // time.h function
    strptime(temp, "%Y-%m-%dT%H-%M-%SZ", &ltm);

    time_t unix_time = mktime(&ltm);

    // report the time to serial for debugging
    if (false)
    {
        Serial.printf("Date and Time: %d-%02d-%02d %02d:%02d:%02d\n",
                      ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
                      ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        Serial.printf("Unix time: %ld\n", unix_time);
    }

    // create start and end event structs
    struct tm event, event2;
    time_t epoch = mktime(&ltm) + (time_t)timeZone * 3600L;
    gmtime_r(&epoch, &event);
    strncpy(dst, asctime(&event) + 11, 5);

    dst[5] = '-';

    strncpy(temp2, to, 16);
    temp2[16] = 0;

    // Same as above

    // https://github.com/esp8266/Arduino/issues/5141, quickfix
    memmove(temp2 + 5, temp2 + 4, 16);
    memmove(temp2 + 8, temp2 + 7, 16);
    memmove(temp2 + 14, temp2 + 13, 16);
    memmove(temp2 + 16, temp2 + 15, 16);
    temp2[4] = temp2[7] = temp2[13] = temp2[16] = '-';

    strptime(temp2, "%Y-%m-%dT%H-%M-%SZ", &ltm2);

    time_t epoch2 = mktime(&ltm2) + (time_t)timeZone * 3600L;
    gmtime_r(&epoch2, &event2);
    strncpy(dst + 6, asctime(&event2) + 11, 5);

    dst[11] = 0;

    // Find UNIX timestamps for next days to see where to put event
    // network.getTime(day0, 0);
    *timeStamp = epoch;

    // // difference to now
    time_t nowSecs = time(nullptr) + (long)timeZone * 3600L;

    //     // calculate difference
    *differenceToNowS = difftime(epoch, nowSecs);

    // date info
    struct tm my_time, my_time2;
    my_time = *localtime(&epoch);
    // printf("Target Date and Time: %d-%02d-%02d %02d:%02d:%02d\n",
    //        my_time.tm_year + 1900, my_time.tm_mon + 1, my_time.tm_mday,
    //        my_time.tm_hour, my_time.tm_min, my_time.tm_sec);

    *time_element = my_time;

    my_time2 = *localtime(&epoch2);
    if (my_time.tm_hour == my_time2.tm_hour && my_time.tm_mday != my_time2.tm_mday)
    { // only for full day events true
        // my_time2.tm_mday = my_time2.tm_mday - 1; // somehow this faulty adds a day
        epoch2 = epoch2 - (time_t)86400;
        my_time2 = *localtime(&epoch2);
    }
    *time_element_end = my_time2;

    *day = 0;
}

// parse .ics (iCal) data to entries format
void parseiCal(int cal_i)
{
    long i = 0;
    long n = strlen(data);

    // determine time now, will be used to determine appointments that are close
    time_t nowSecs = time(nullptr) + (long)timeZone * 3600L;
    struct tm *time_now_element;
    time_now_element = localtime(&nowSecs);

    // determine end date to look for dates
    time_t inFourDaysS = time(nullptr) + (long)timeZone * 3600L + 4 * 24 * 60 * 60;
    struct tm *time_inFourDays_element;
    time_inFourDays_element = localtime(&inFourDaysS);

    // Search raw data for events
    while (i < n && strstr(data + i, "BEGIN:VEVENT"))
    {
        // Find next event start and end
        i = strstr(data + i, "BEGIN:VEVENT") - data + 12;
        char *end = strstr(data + i, "END:VEVENT");

        if (end == NULL)
            continue;

        // Find all relevant event data (pointers)
        char *summary = strstr(data + i, "SUMMARY:") + 8;
        char *location = strstr(data + i, "LOCATION:") + 9;
        char *timeStart = strstr(data + i, "DTSTART:") + 8;
        char *fullDate = strstr(data + i, "DTSTART;VALUE=DATE:") + 19; // full day will be like this "DTSTART;VALUE=DATE:20231113"
        char *timeEnd = strstr(data + i, "DTEND:") + 6;
        char *fullDateEnd = strstr(data + i, "DTEND;VALUE=DATE:") + 17; // full day will be like this "DTEND;VALUE=DATE:20231113"
        char *yearlyRecurring = strstr(data + i, "YEARLY");             // depending on which code wrote it can be "RRULE:FREQ=YEARLY" or "RRULE:FREQ=YEARLY;INTERVAL=1"

        if (summary && summary < end && (summary - data) > 0)
        {
            strncpy(entries[entriesNum].name, summary, strchr(summary, '\n') - summary);
            entries[entriesNum].name[strchr(summary, '\n') - summary] = 0;
        }
        if (location && location < end && (location - data) > 0)
        {
            strncpy(entries[entriesNum].location, location, strchr(location, '\n') - location);
            entries[entriesNum].location[strchr(location, '\n') - location] = 0;
        }

        // reformat time information
        if (fullDate && fullDate < end && (fullDate - data) > 0) // we need to handle full dates differently
        {
            // bring these dates to the same format as normal appointments
            char fakeDate[24];
            strncpy(fakeDate, fullDate, strchr(fullDate, '\n') - fullDate);
            char dummyTime[] = "T000000Z";
            strcpy(fakeDate + 8, dummyTime);

            char fakeDateEnd[24];
            strncpy(fakeDateEnd, fullDateEnd, strchr(fullDateEnd, '\n') - fullDateEnd);
            strcpy(fakeDateEnd + 8, dummyTime);

            // append time information
            getToFrom(entries[entriesNum].time, fakeDate, fakeDateEnd, &entries[entriesNum].day,
                      &entries[entriesNum].timeStamp, &entries[entriesNum].differenceToNowS,
                      &entries[entriesNum].time_element, &entries[entriesNum].time_element_end);

            entries[entriesNum].isFullDay = true;
        }
        else
        {
            if (timeStart && timeStart < end && timeEnd < end)
            {
                getToFrom(entries[entriesNum].time, timeStart, timeEnd, &entries[entriesNum].day,
                          &entries[entriesNum].timeStamp, &entries[entriesNum].differenceToNowS,
                          &entries[entriesNum].time_element, &entries[entriesNum].time_element_end);
            }
            entries[entriesNum].isFullDay = false;
        }

        // yearly recurring events
        if (yearlyRecurring && yearlyRecurring < end && (yearlyRecurring - data) > 0)
        {
            // we fake the year and the time
            entries[entriesNum].time_element.tm_year = dates_to_do[0].tm_year;
            entries[entriesNum].time_element_end.tm_year = dates_to_do[0].tm_year;
        }

        // append calender identifier
        entries[entriesNum].calID = cal_i;

        // preselection to keep entries small
        //   (isDateInRange(dates_to_do[0], entries[entriesNum].time_element, entries[entriesNum].time_element_end))////old: (entries[entriesNum].differenceToNowS > -(24 * 60 * 60) && entries[entriesNum].differenceToNowS < MAX_TIME_WINDOW)
        if (isDateInRange(dates_to_do[0], entries[entriesNum].time_element, entries[entriesNum].time_element_end) ||
            isDateInRange(dates_to_do[1], entries[entriesNum].time_element, entries[entriesNum].time_element_end) ||
            isDateInRange(dates_to_do[2], entries[entriesNum].time_element, entries[entriesNum].time_element_end) ||
            isDateInRange(dates_to_do[3], entries[entriesNum].time_element, entries[entriesNum].time_element_end))
        {
            // Serial.println("within timeframe");
            ++entriesNum; // keeps on increasing between calendars
            if (entriesNum >= size_entriesNum)
            {
                Serial.println("entriesNum too high, aborting");
                break; // prevent overflow
            }
        }
        else
        {
            // event already past or too far in the future, skip advancing the counter entriesNum, such that it will
            // overwritten in the next iteration. This should prevent entries from being cluttered up.
        }
    }

    // Serial.print("entriesNum: ");
    // Serial.println(entriesNum);

    // Sort entries by time
    qsort(entries, entriesNum, sizeof(entry), cmp);
}

// draw events for a speciefied date
void drawCalendarforDate(struct tm targetDate, int x_start, int y_start)
{

    int counter_printed = 0; // keep track of how many were printed
    int line_height = 25;
    int max_height = layout_YForecastHeight - 120; // line_height * 5; // stop writing when this is reached

    int xi = x_start, yi = y_start;
    //    display.setFont(&FreeSans12pt7b);
    display.setFont(&FreeSerif12pt7b);
    display.setCursor(xi, yi);
    display.setTextColor(color_textNormal, 7);

    // go through all entries and plot the one with a fitting date
    for (int i = 0; i < entriesNum; i++) // was ++i
    {

        // check if wihtin timeframe (not earlier than today and not far in the furutre)
        // isDateInRange(targetDate, entries[i].time_element, entries[i].time_element_end)
        if (isDateInRange(targetDate, entries[i].time_element, entries[i].time_element_end)) // before: (isSameDay(entries[i].time_element, targetDate))
        {

            if (display.getCursorY() - y_start < max_height)
            {
                display.setCursor(xi, yi);

                if (entries[i].isFullDay)
                { // time only of not fullday event, I dont know why, but this works
                    // display.print("       ");

                    // for multi-day events
                    if (!(isSameDay(entries[i].time_element, entries[i].time_element_end)))
                    {
                        if (isSameDay(targetDate, entries[i].time_element))
                        {
                            display.print("["); // this is the first day
                        }
                        else
                        {
                            display.print("..."); // there where days before this
                        }
                    }
                }
                else
                {
                    // time as (12:10)
                    display.print("("); // tab in for not-fullday
                    display.print(entries[i].time);
                    display.print(") ");
                }

                // calender ID
                if (true)
                {
                    display.setFont(&FreeSansBold12pt7b);
                    display.print("");
                    display.print(calNames[entries[i].calID]);
                    display.print("");
                    display.setFont(&FreeSerif12pt7b);
                }

                // entry name
                display.print(" ");
                display.print(entries[i].name);

                // for multi-day events
                if (!(isSameDay(entries[i].time_element, entries[i].time_element_end)))
                {
                    if (isSameDay(targetDate, entries[i].time_element_end))
                    {
                        display.print("]"); // this is the last day
                    }
                    else
                    {
                        display.print("..."); // there are more days to come
                    }
                }

                // update cursor
                yi = yi + line_height;
                counter_printed++;

                // debugging
                // char debug_output[100];
                // sprintf(debug_output, "name %s, start day %d, end day %d",
                //         entries[i].name, entries[i].time_element.tm_mday, entries[i].time_element_end.tm_mday);
                // Serial.println(debug_output);
            }
            else
            {
                display.setCursor(xi, yi);
                display.println("      (...)");
            }
        }
        else
        {
            // date not in range

            // debugging
            // char debug_output[100];
            // sprintf(debug_output, " NOT IN RANGE name %s, start day %d, end day %d",
            //         entries[i].name, entries[i].time_element.tm_mday, entries[i].time_element_end.tm_mday);
            // Serial.println(debug_output);
        }
    } // go through all entries

    if (counter_printed == 0)
    {
        display.println("--- keine Termine ---");
    }
}

bool isSameDay(const struct tm &time1, const struct tm &time2)
{
    return (time1.tm_year == time2.tm_year) &&
           (time1.tm_mon == time2.tm_mon) &&
           (time1.tm_mday == time2.tm_mday);
}

bool isDateInRange(struct tm checkDate, struct tm startDate, struct tm endDate)
{
    time_t checkTime = mktime(&checkDate);
    time_t startTime = mktime(&startDate);
    time_t endTime = mktime(&endDate);

    bool bool_out;

    // checkDate == startDate
    if (isSameDay(checkDate, startDate) || isSameDay(checkDate, endDate))
    {
        bool_out = true;
    }
    else
    {
        bool_out = (difftime(checkTime, startTime) >= 0 && difftime(endTime, checkTime) >= 0);
    }

    return bool_out;
}

// Struct event comparison function, by timestamp, used for qsort later on
int cmp(const void *a, const void *b)
{
    entry *entryA = (entry *)a;
    entry *entryB = (entry *)b;

    return (entryA->timeStamp - entryB->timeStamp);
}

// Function for drawing weather info
void alignText(const char align, const char *text, int16_t x, int16_t y)
{
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    switch (align)
    {
    case CENTRE_BOT:
    case CENTRE:
    case CENTRE_TOP:
        x1 = x - w / 2;
        break;
    case RIGHT_BOT:
    case RIGHT:
    case RIGHT_TOP:
        x1 = x - w;
        break;
    case LEFT_BOT:
    case LEFT:
    case LEFT_TOP:
    default:
        x1 = x;
        break;
    }
    switch (align)
    {
    case CENTRE_BOT:
    case RIGHT_BOT:
    case LEFT_BOT:
        y1 = y;
        break;
    case CENTRE:
    case RIGHT:
    case LEFT:
        y1 = y + h / 2;
        break;
    case CENTRE_TOP:
    case LEFT_TOP:
    case RIGHT_TOP:
    default:
        y1 = y + h;
        break;
    }
    display.setCursor(x1, y1);
    display.print(text);
}

void drawWeather(char WeatherAbbr[], int x, int y)
{
    // Searching for weather state abbreviation
    for (int i = 0; i < 18; ++i)
    {
        // If found draw specified icon
        if (strcmp(abbrs[i], WeatherAbbr) == 0)
            display.drawBitmap(x, y, logos[i], 152, 152, color_images); // was logos
    }
}

// Function for drawing weather info
void drawForecast()
{

    int xOffset = 10;
    int startDay = 1;
    int numOfDays = (sizeof(OWOC.forecast) / sizeof(OWOC.forecast[0]));

    for (int day_i = startDay; day_i < 4; day_i++) // 0 is today
    {
        // determine x-coordinates for displaying
        int textCentre = layout_XForecast + layout_Margin + 80;

        int dayOffset = layout_YForecastTopOffset + (day_i - startDay + 0.2) * layout_YForecastHeight; // vertical position

        // image of weather
        char icontd[8] = "10d";
        sprintf(icontd, "%s", OWOC.forecast[day_i].icon);
        drawWeather(icontd, textCentre - 71, dayOffset - 45);

        display.setFont(&FreeSans9pt7b);

        // min and max temperature
        sprintf(Output, "%02.0f / %02.0f C", OWOC.forecast[day_i].temperatureLow, OWOC.forecast[day_i].temperatureHigh);
        alignText(CENTRE_BOT, Output, textCentre, dayOffset + 100);

        // rain
        sprintf(Output, "%02.0f%%, %02.01f mm", OWOC.forecast[day_i].pop * 100, OWOC.forecast[day_i].rainVolume + OWOC.forecast[day_i].snowVolume);
        alignText(CENTRE_BOT, Output, textCentre, dayOffset + 120);

        // Weekday in short form
        display.setTextColor(color_textNormal, WHITE);
        display.setTextSize(1);
        display.setFont(&FreeSans12pt7b);
        sprintf(Output, "%s, %02d.%02d.%04d", dayShortStr(weekday(OWOC.forecast[day_i].dayTime)), dates_to_do[day_i].tm_mday, dates_to_do[day_i].tm_mon + 1, dates_to_do[day_i].tm_year + 1900); // careful months go from 0 to 11 !

        alignText(LEFT_BOT, Output, layout_XForecastCalendarStart, layout_YForecastTopOffset + (day_i - startDay) * layout_YForecastHeight - 5);
    }
}

void drawCalendarforForecastDays()
{
    int xOffset = 10;
    int startDay = 1;

    for (int day_i = startDay; day_i < 4; day_i++) // 0 is today
    {
        // determine x-coordinates for displaying
        int textCentre = layout_XForecast + layout_Margin + 80;

        int dayOffset = layout_YForecastTopOffset + (day_i - startDay + 0.2) * layout_YForecastHeight; // vertical position

        // Weekday in short form
        display.setTextColor(color_textNormal, WHITE);
        display.setTextSize(1);
        display.setFont(&FreeSans12pt7b);
        sprintf(Output, "%s, %02d.%02d.%04d", dayShortStr(weekday(OWOC.forecast[day_i].dayTime)), dates_to_do[day_i].tm_mday, dates_to_do[day_i].tm_mon + 1, dates_to_do[day_i].tm_year + 1900); // careful months go from 0 to 11 !

        alignText(LEFT_BOT, Output, layout_XForecastCalendarStart, layout_YForecastTopOffset + (day_i - startDay) * layout_YForecastHeight - 5);

        // draw calendar stuff
        drawCalendarforDate(dates_to_do[day_i], layout_XForecastCalendarStart, layout_YForecastTopOffset + (day_i - startDay) * layout_YForecastHeight + 40);
    }
}

// Function for drawing current time
void drawTime()
{
    int textWidth;
    char Output2[200] = {0};

    t = now();
    // Drawing current time

    display.setTextColor(7, 2);
    display.setFont(&Roboto_Light_120);
    display.setTextSize(1);

    // background color
    display.fillRect(0, 0, layout_XForecast, layout_YDateBoxHeight, 2);

    // time
    // sprintf(Output, "%02d:%02d", hour(t), minute(t));
    // alignText(CENTRE_BOT, Output, 840, 154);
    // Serial.print(Output);
    // Serial.print(" ");

    // date
    display.setFont(&Roboto_Light_48);
    sprintf(Output, "%s %02d ", dayShortStr(weekday(t)) + 0, day());
    sprintf(Output2, "%s %04d", monthShortStr(month(t)) + 0, year());
    strcat(Output, Output2);
    alignText(CENTRE_BOT, Output, layout_XForecast / 2, layout_YDateBoxHeight / 2 + 10);

    Serial.println(Output);
}

int sign(int x)
{
    if (x > 0)
    {
        return 1;
    }
    else if (x < 0)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

// hourly forecast in plot
void drawHourly()
{
    const int yTop = 251;
    const int yHeight = 150;
    int xMargin = 55;
    const int xLeft = 0 + xMargin;
    int xWidth = layout_XForecast - xMargin * 2;
    const int hoursDisplay = 24; // how many hours in the forecast
    const int hourPitch = xWidth / hoursDisplay;

    xWidth = hoursDisplay * hourPitch;              // update for nice fit
    xMargin = (layout_XForecast - xWidth) / 2 + 10; // should put it nicely in the middle
    const float minPrec = 0;
    float maxPrec = 5; // this sets the minimum max value of the rain axis
    float maxTemp = 25;
    float minTemp = 0;

    display.setFont(&FreeSans9pt7b);

    // determine max and min values of rain and temperature
    for (int Houry = 0; Houry < hoursDisplay; Houry++)
    {
        float thisPrec = OWOC.hour[Houry].rainVolume + OWOC.hour[Houry].snowVolume;
        if (maxPrec < thisPrec)
            maxPrec = thisPrec;
        if (maxTemp < OWOC.hour[Houry].temperature)
            maxTemp = OWOC.hour[Houry].temperature;
        if (minTemp > OWOC.hour[Houry].temperature)
            minTemp = OWOC.hour[Houry].temperature;
    }

    // shade for the  (not current!) night
    for (int day_td = 0; day_td < 2; day_td++)
    {

        int relHourSunset = difftime(OWOC.forecast[day_td + 0].sunsetTime, OWOC.current->dayTime) / (3600);
        int relHourSunrise = difftime(OWOC.forecast[day_td + 1].sunriseTime, OWOC.current->dayTime) / (3600);

        // cap to bounds
        if (relHourSunset < 0)
        {
            relHourSunset = 0;
        }
        if (relHourSunset > hoursDisplay)
        {
            relHourSunset = hoursDisplay;
        }
        if (relHourSunrise < 0)
        {
            relHourSunrise = 0;
        }
        if (relHourSunrise > hoursDisplay)
        {
            relHourSunrise = hoursDisplay;
        }

        if (relHourSunrise != relHourSunset) // check if not both are out-of-bounds
        {
            display.fillRect(xLeft + (relHourSunset * hourPitch),          // x start
                             yTop,                                         // y start
                             (relHourSunrise - relHourSunset) * hourPitch, // width
                             yHeight,                                      // height
                             0xf7be);                                      // color, RGB565 https://rgbcolorpicker.com/565
        }
    }

    //  make grid and hour labels
    for (int Houry = 0; Houry <= hoursDisplay; Houry++)
    {
        long time_Houry = hour(OWOC.hour[Houry].dayTime + (timeZone) * 3600L); // .dayTime is in GMT time (-1 for current hour???)

        // display.drawLine(xLeft + (Houry * hourPitch), yTop, xLeft + (Houry * hourPitch), yTop + yHeight, BLACK); // vertical tick lines

        if (time_Houry == 0) // ? change of day, vertical
        {
            display.drawLine(xLeft + (Houry * hourPitch), yTop, xLeft + (Houry * hourPitch), yTop + yHeight, color_textNormal); // vertical tick lines
        }

        if (!(Houry % 4))
        {
            display.setTextColor(color_textUnNormal);
            sprintf(Output, "%2d", time_Houry);
            alignText(CENTRE_TOP, Output, xLeft + (Houry * hourPitch), yTop + yHeight + 2);
        }
    }

    // (?) draw box
    display.setTextColor(color_textNormal);
    display.drawLine(xLeft, yTop + yHeight, xLeft + xWidth, yTop + yHeight, color_textNormal); // bottom line
    //  display.drawLine(xLeft, yTop, xLeft + xWidth, yTop, color_textNormal);

    // sprintf(Output, "%02.0f", minPrec);
    // alignText(LEFT, Output, xLeft + xWidth + 5, yTop + yHeight);
    display.setTextColor(color_textUnNormal);
    sprintf(Output, "%02.0f", (maxPrec + .499));
    alignText(LEFT, Output, xLeft + xWidth + 5, yTop);
    sprintf(Output, "mm/h");
    alignText(LEFT, Output, xLeft + xWidth + 5, yTop + 20);

    // horizontal guide lines
    float yTempScale = (yHeight / (round(maxTemp + 0.499) - round(minTemp - 0.5)));

    int dTemperature_lines = 5; // °C
    // int num_temperature_lines = (round(maxTemp + 0.499) - round(minTemp - 0.5)) / dTemperature_lines;
    // float yTempScale_lines = (yHeight/num_temperature_lines);

    // for (int Houry = 0; Houry <= num_temperature_lines; Houry++)
    // {
    //     display.drawLine(xLeft, yTop + (Houry * yTempScale_lines), xLeft + xWidth, // was xLeft + xWidth,
    //      yTop + (Houry * yTempScale_lines), color_textNormal);
    // }

    // decide tempertures to draw a line for
    int yTempScale_temperautres[20];
    // yTempScale_temperautres[0] = maxTemp; // start at the top, because thats the way its plotted
    int dT_counter = 0;

    for (int dT_i = floor(maxTemp / dTemperature_lines) * dTemperature_lines; dT_i >= minTemp; dT_i = dT_i - dTemperature_lines)
    {
        yTempScale_temperautres[dT_counter] = dT_i;
        dT_counter++;
    }

    // draw the horizontal temperature guide lines
    for (int line_i = 0; line_i < dT_counter; line_i++)
    {

        float yPixel_Temp_guide = yTop + (int)(yTempScale * (round(maxTemp + 0.499) - yTempScale_temperautres[line_i]));
        display.drawLine(xLeft, yPixel_Temp_guide, xLeft + xWidth, // was xLeft + xWidth,
                         yPixel_Temp_guide, color_textNormal);

        // add labels
        if (yTempScale_temperautres[line_i] % 10 == 0)
        {
            display.setTextColor(color_textUnNormal);
            // if (maxTemp-yTempScale_temperautres[line_i]>10 && yTempScale_temperautres[line_i]-minTemp>10){ // make sure its not to
            sprintf(Output, "%02.0f", (yTempScale_temperautres[line_i] + 0.499));
            alignText(RIGHT, Output, xLeft - 5, yPixel_Temp_guide);
            // }
        }
    }

    // draw temperature and rain line/bars
    for (int Houry = 0; Houry <= (hoursDisplay - 1); Houry++)
    {
        display.drawThickLine(xLeft + (Houry * hourPitch),
                              yTop + (int)(yTempScale * (round(maxTemp + 0.499) - (OWOC.hour[Houry].temperature))),
                              xLeft + ((Houry + 1) * hourPitch),
                              yTop + (int)(yTempScale * (round(maxTemp + 0.499) - (OWOC.hour[Houry + 1].temperature))),
                              color_textNormal + 1, 2);
        float yPrecScale = (yHeight / (round(maxPrec + 0.499)));
        float thisPrec = OWOC.hour[Houry].rainVolume + OWOC.hour[Houry].snowVolume;
        display.fillRect(xLeft + (Houry * hourPitch) + round(hourPitch / 3),
                         yTop + (int)(yPrecScale * (round(maxPrec + .499) - thisPrec)), round(hourPitch / 3),
                         (int)(yPrecScale * thisPrec), color_textNormal + 1);
    }
}

// Current weather drawing function
void drawCurrent()
{

    // image of current weather
    char icontd[8] = "10d";
    sprintf(icontd, "%s", OWOC.current->icon);
    Serial.print("Icon to print: ");
    Serial.println(icontd);
    drawWeather(icontd, 6, layout_YTodayWeatherIcons - 63);

    // current temperature and max min of the day
    display.setFont(&FreeSans24pt7b);
    // display.setTextSize(2);
    display.setTextColor(color_textNormal, 7);
    sprintf(Output, "%02.01f 'C", OWOC.current->temperature);
    alignText(CENTRE_BOT, Output, layout_XForecast / 2 - 20, layout_YWeather);

    // display.setFont(&Roboto_Light_36);
    display.setFont(&FreeSans18pt7b);
    sprintf(Output, "(%02.0f / %02.0f 'C) ", OWOC.forecast[0].temperatureLow, OWOC.forecast[0].temperatureHigh);
    alignText(CENTRE_BOT, Output, layout_XForecast / 2 - 20, layout_YWeather + layout_LineHeight * 2);

    // display.setFont(&FreeSans12pt7b);
    // sprintf(Output, "%02d:%02d-%02d:%02d", hour(OWOC.current.sunrise), minute(OWOC.current.sunrise),
    //         hour(OWOC.current.sunset), minute(OWOC.current.sunset));
    // alignText(CENTRE_BOT, Output, 150, 340);

    // sprintf(Output, "%04.0fhPa", OWOC.current.pressure);
    // alignText(CENTRE_BOT, Output, 150, 405);

    // sprintf(Output, "%03.0f%% Rh", OWOC.current.humidity);
    // alignText(CENTRE_BOT, Output, 150, 446);
}

/*
  return value is percent of moon cycle ( from 0.0 to 0.999999), i.e.:

  0.0: New Moon
  0.125: Waxing Crescent Moon
  0.25: Quarter Moon
  0.375: Waxing Gibbous Moon
  0.5: Full Moon
  0.625: Waning Gibbous Moon
  0.75: Last Quarter Moon
  0.875: Waning Crescent Moon

*/
float getMoonPhase(time_t tdate)
{
    time_t newmoonref = 1263539460; // known new moon date (2010-01-15 07:11)
    // moon phase is 29.5305882 days, which is 2551442.82048 seconds
    float phase = abs(tdate - newmoonref) / (double)2551442.82048;
    phase -= (int)phase; // leave only the remainder
    if (newmoonref > tdate)
        phase = 1 - phase;
    return phase;
}

// Function for drawing the moon
void drawMoon()
{
    const int MoonCentreX = 3.2 * layout_XForecast / 4;
    const int MoonCentreY = layout_YTodayWeatherIcons;
    const int MoonBox = 35;
    float moonphase = getMoonPhase(now());
    int moonage = 29.5305882 * moonphase;
    // Serial.println("moon age: " + String(moonage));
    // convert to appropriate icon
    display.setFont(&moon_Phases36pt7b);
    sprintf(Output, "%c", (char)((int)'A' + (int)(moonage * 25. / 30)));
    display.setTextColor(color_images, 7);
    alignText(CENTRE, Output, MoonCentreX, MoonCentreY);

    display.setFont(&FreeSans12pt7b);
    sprintf(Output, "%02d days old", moonage);
    display.setTextColor(color_images, 7);
    alignText(CENTRE_TOP, Output, MoonCentreX, MoonCentreY + MoonBox - 5);
    // int currentphase = moonphase * 28. + .5;
    // alignText(CENTRE_TOP, moonphasenames[currentphase], MoonCentreX, MoonCentreY + MoonBox + 20);
}

/**
 * Make a test API call to see if we have a valid API key
 *
 * Older keys made for OpenWeather 2.5 OneCall API work, while newer ones won't work, due to the service becoming
 * deprecated.
 */
bool checkIfAPIKeyIsValid(char *ONECALLKEY)
{
    bool failed = false;

    // Create the buffer for holding the API response, make it large enough just in case
    char *data;
    data = (char *)ps_malloc(50000LL);

    // Http object used to make GET request
    HTTPClient http;
    http.getStream().setTimeout(10);
    http.getStream().flush();
    http.getStream().setNoDelay(true);

    // Combine the base URL and the API key to do a test call
    char *baseURL = "https://api.openweathermap.org/data/3.0/onecall?lat=45.560001&lon=18.675880&units=metric&appid=";
    char apiTestURL[200];
    sprintf(apiTestURL, "%s%s", baseURL, ONECALLKEY);

    // Begin http by passing url to it
    http.begin(apiTestURL);

    delay(300);

    // Download data until it's a verified complete download
    // Actually do request
    int httpCode = http.GET();

    if (httpCode == 200)
    {
        long n = 0;

        long now = millis();

        while (millis() - now < 1000)
        {
            while (http.getStream().available())
            {
                data[n++] = http.getStream().read();
                now = millis();
            }
        }

        data[n++] = 0;

        // If the reply constains this string - it's invalid
        if (strstr(data, "Invalid API key.") != NULL)
            failed = true;
    }
    else
    {
        // In case there was another HTTP code, it failed
        Serial.print("Error! HTTP Code: ");
        Serial.println(httpCode);
        failed = true;
    }

    // End http
    http.end();

    // Free the memory
    free(data);

    return !failed;
}
