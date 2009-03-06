#include "gps.h"
#include "board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "loggerHardware.h"
#include "usart.h"
#include "modp_numtoa.h"
#include "usb_comm.h"
#include <stdlib.h>
#include <string.h>


#define GPS_DATA_LINE_BUFFER_LEN 	200
#define GPS_TASK_PRIORITY 			( tskIDLE_PRIORITY + 1 )
#define GPS_TASK_STACK_SIZE			100


#define GPS_QUALITY_NO_FIX 0
#define GPS_QUALITY_SPS 1
#define GPS_QUALITY_DIFFERENTIAL 2

#define GPS_LOCK_FLASH_COUNT 2
#define GPS_NOFIX_FLASH_COUNT 10

#define LATITUDE_DATA_LEN 12
#define LATITUDE_STR_RAW_BUFFER_LEN LATITUDE_DATA_LEN + 1
 
#define LONGITUDE_DATA_LEN 13
#define LONGITUDE_STR_RAW_BUFFER_LEN LONGITUDE_DATA_LEN + 1

#define UTC_TIME_BUFFER_LEN 11
#define UTC_VELOCITY_BUFFER_LEN 10

char g_GPSdataLine[GPS_DATA_LINE_BUFFER_LEN];

float	g_latitude;
float 	g_longitude;

float	g_latitudeRaw;
float	g_longitudeRaw;



float	g_UTCTime;
char 	g_UTCTimeString[UTC_TIME_BUFFER_LEN];

float	g_velocity;
char  	g_velocityString[UTC_VELOCITY_BUFFER_LEN];

int		g_gpsQuality;
int		g_satellitesUsedForPosition;
int		g_gpsPositionUpdated;

int		g_gpsVelocityUpdated;

float getUTCTime(){
	return g_UTCTime;
}

char * getUTCTimeString(){
	return g_UTCTimeString;
}

void getUTCTimeFormatted(char * buf){
	
}

float getLatitude(){
	return g_latitude;
}

float getLatitudeRaw(){
	return g_latitudeRaw;
}

float getLongitude(){
	return g_longitude;
}

float getLongitudeRaw(){
	return g_longitudeRaw;
}

int getGPSQuality(){
	return g_gpsQuality;
}

int getSatellitesUsedForPosition(){
	return g_satellitesUsedForPosition;
}

int getGPSPositionUpdated(){
	return g_gpsPositionUpdated;
}

void setGPSPositionStale(){
	g_gpsPositionUpdated = 0;	
}

float getGPSVelocity(){
	return g_velocity;
}

char * getGPSVelocityString(){
	return g_velocityString;	
}

char * getGPSDataLine(){
	return g_GPSdataLine;
}

int getGPSVelocityUpdated(){
	return g_gpsVelocityUpdated;
}

void setGPSVelocityStale(){
	g_gpsVelocityUpdated = 0;
}

void startGPSTask(){
	g_latitude = 0.0;
	g_longitude = 0.0;
	g_UTCTime = 0.0;
	g_gpsQuality = GPS_QUALITY_NO_FIX;
	g_satellitesUsedForPosition = 0;
	g_gpsPositionUpdated = 0;
	g_velocity = 0.0;
	g_gpsVelocityUpdated = 0;
	
	
	xTaskCreate( GPSTask, ( signed portCHAR * ) "GPSTask", GPS_TASK_STACK_SIZE, NULL, 	GPS_TASK_PRIORITY, 	NULL );
}

void GPSTask( void *pvParameters ){
	
	int flashCount = 0;
	for( ;; )
	{
		int len = usart1_readLine(g_GPSdataLine, GPS_DATA_LINE_BUFFER_LEN);
		if (len > 0){
			if (*g_GPSdataLine == '$' && *(g_GPSdataLine + 1) =='G' && *(g_GPSdataLine + 2) == 'P'){
				char * data = g_GPSdataLine + 3;
				if (strstr(data,"GGA,")){
					parseGGA(data + 4);
					if (flashCount == 0) DisableLED(LED1);
					flashCount++;
					int targetFlashCount = (g_gpsQuality == GPS_QUALITY_NO_FIX ? GPS_NOFIX_FLASH_COUNT: GPS_LOCK_FLASH_COUNT);
					if (flashCount >= targetFlashCount){
						EnableLED(LED1);
						flashCount = 0;		
					}
				} else if (strstr(data,"GSA,")){ //GPS Fix data
					parseGSA(data + 4);						
				} else if (strstr(data,"GSV,")){ //Satellites in view
					parseGSV(data + 4);					
				} else if (strstr(data,"RMC,")){ //Recommended Minimum Specific GNSS Data
					parseRMC(data + 4);					
				} else if (strstr(data,"VTG,")){ //Course Over Ground and Ground Speed
					parseVTG(data + 4);					
				} else if (strstr(data,"GLL,")){ //Geographic Position � Latitude/Longitude
					parseGLL(data + 4);					
				} else if (strstr(data,"ZDA,")){ //Time & Date
					parseZDA(data + 4);
				}
			}
		}
	}
}

//Parse Global Positioning System Fix Data.
void parseGGA(char *data){

	//SendString(data);
	
	char * delim = strchr(data,',');
	int param = 0;
	
	float latitude = 0.0;
	float latitudeRaw = 0.0;
	float longitude = 0.0;
	float longitudeRaw = 0.0;
	
	while (delim != NULL){
		*delim = '\0';
		switch (param){
			case 0:
				{
					unsigned int len = strlen(data);
					if (len > 0 && len < UTC_VELOCITY_BUFFER_LEN){
						strcpy(g_UTCTimeString,data);
						g_UTCTime = atof(data);
					}
				}
				break;
			case 1:
				{
					unsigned int len = strlen(data);
					if ( len > 0 && len <= LATITUDE_DATA_LEN ){
						//Format is ddmm.mmmmmm
						char degreesStr[3];
						latitudeRaw = atof(data);
						
						//SendString(data);
						//SendString(",");
						//SendFloat(latitudeRaw,6);
						//SendString(",");
						
						strncpy(degreesStr, data, 2);
						degreesStr[2] = 0;
						float minutes = atof(data + 2);
						minutes = minutes / 60.0;
						latitude = ((float)atoi(degreesStr)) + minutes;
					}
					else{
						latitude = 0;
						//TODO log error	
					}
				}
				break;
			case 2:
				{
					if (data[0] == 'S'){
						latitude = -latitude;
						latitudeRaw = -latitudeRaw;
					}
				}
				break;
			case 3:
				{	
					unsigned int len = strlen(data);
					if ( len > 0 && len <= LONGITUDE_DATA_LEN ){
						//Format is dddmm.mmmmmm
						char degreesStr[4];
						longitudeRaw = atof(data);

						//SendString(data);
						//SendString(",");
						//SendFloat(longitudeRaw,6);
						//SendString("\r\n");

						strncpy(degreesStr, data, 3);
						degreesStr[3] = 0;
						float minutes = atof(data + 3);
						minutes = minutes / 60.0;
						longitude = ((float)atoi(degreesStr)) + minutes;						
					}
					else{
						longitude = 0;
						//TODO log error	
					}
				}
				break;
			case 4:
				{
					if (data[0] == 'W'){
						longitude = -longitude;
						longitudeRaw = -longitudeRaw;
					}	
				}
				break;
			case 5:
				g_gpsQuality = atoi(data);
				break;
			case 6:
				g_satellitesUsedForPosition = atoi(data);
				break;
		}
		param++;
		data = delim + 1;
		delim = strchr(data,',');
	}

	g_longitude = longitude;
	g_longitudeRaw = longitudeRaw;
	
	g_latitude = latitude;
	g_latitudeRaw = latitudeRaw;
	
	if (g_gpsQuality != GPS_QUALITY_NO_FIX) g_gpsPositionUpdated = 1;
}

//Parse GNSS DOP and Active Satellites
void parseGSA(char *data){
	
}

//Parse Course Over Ground and Ground Speed
void parseVTG(char *data){

	char * delim = strchr(data,',');
	int param = 0;
	
	while (delim != NULL){
		*delim = '\0';
		switch (param){
			case 6: //Speed over ground
				{
					if (strlen(data) >= 1){
						g_velocity = (float)atof(data);
						strcpy(g_velocityString,data);
					}
				}
				break;
			default:
				break;
		}
		param++;
		data = delim + 1;
		delim = strchr(data,',');
	}
	if (g_gpsQuality != GPS_QUALITY_NO_FIX) g_gpsVelocityUpdated = 1;
}

//Parse Geographic Position � Latitude / Longitude
void parseGLL(char *data){
	
}

//Parse Time & Date
void parseZDA(char *data){
	
}

//Parse GNSS Satellites in View
void parseGSV(char *data){

}

//Parse Recommended Minimum Navigation Information
void parseRMC(char *data){
	
}
