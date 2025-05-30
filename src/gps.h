#ifndef _GPS_H_
#define _GPS_H_
#include <Arduino.h>
#include "config.h"

// 此处为了兼容其他的多串口Arduino板子
#define GpsSerial Serial
#define DebugSerial Serial



// 初始化GPS
void gpsInit();
// 错误记录
void errorLog(int num);
// 打印GPS数据
void printGpsBuffer();
// 解析GPS数据
void parseGpsBuffer();
// GPS读取函数
void gpsRead();
// 清空GPS接收缓存区
void clrGpsRxBuffer(void);
// 将度分数据转换成十进制数据
double gpsDataConvert(char *degreeString);

int L = 13; // LED指示灯引脚

struct
{
    char GPS_Buffer[80];
    bool isGetData;     // 是否获取到GPS数据
    bool isParseData;   // 是否解析完成
    char UTCTime[11];   // UTC时间
    char latitude[11];  // 纬度
    char N_S[2];        // N/S
    char longitude[12]; // 经度
    char E_W[2];        // E/W
    bool isUsefull;     // 定位信息是否有效
} Save_Data;

const unsigned int gpsRxBufferLength = 600;
char gpsRxBuffer[gpsRxBufferLength];
unsigned int ii = 0;

// GPS初始化
void gpsInit()
{
    Serial.println("初始化 GPS 模块...");

    GpsSerial.begin(115200); // 定义波特率9600
    DebugSerial.begin(115200);
    DebugSerial.println("waaax_tek");
    DebugSerial.println("Wating...");

    Save_Data.isGetData = false;
    Save_Data.isParseData = false;
    Save_Data.isUsefull = false;

    Serial.println("GPS 初始化完成");
}

// 错误记录
void errorLog(int num)
{
    DebugSerial.print("ERROR");
    DebugSerial.println(num);
    while (1)
    {
        digitalWrite(L, HIGH);
        delay(300);
        digitalWrite(L, LOW);
        delay(300);
    }
}

// 打印GPS数据
void printGpsBuffer()
{
    if (Save_Data.isParseData)
    {
        Save_Data.isParseData = false;

        DebugSerial.print("Save_Data.UTCTime = ");
        DebugSerial.println(Save_Data.UTCTime);

        if (Save_Data.isUsefull)
        {
            Save_Data.isUsefull = false;
            DebugSerial.print("Save_Data.latitude = ");
            DebugSerial.println(Save_Data.latitude); // 纬度
            DebugSerial.print("Save_Data.N_S = ");
            DebugSerial.println(Save_Data.N_S);
            DebugSerial.print("Save_Data.longitude = "); // 经度
            DebugSerial.println(Save_Data.longitude);
            DebugSerial.print("Save_Data.E_W = ");
            DebugSerial.println(Save_Data.E_W);
        }
        else
        {
            DebugSerial.println("GPS DATA is not usefull!");
        }
    }
}

// 解析GPS数据
void parseGpsBuffer()
{
    char *subString;
    char *subStringNext;
    if (Save_Data.isGetData)
    {
        Save_Data.isGetData = false;
        DebugSerial.println("**************");
        DebugSerial.println(Save_Data.GPS_Buffer);

        for (int i = 0; i <= 6; i++)
        {
            if (i == 0)
            {
                if ((subString = strstr(Save_Data.GPS_Buffer, ",")) == NULL)
                    errorLog(1); // 解析错误
            }
            else
            {
                subString++;
                if ((subStringNext = strstr(subString, ",")) != NULL)
                {
                    char usefullBuffer[2];
                    switch (i)
                    {
                    case 1:
                        memcpy(Save_Data.UTCTime, subString, subStringNext - subString);
                        break; // 获取UTC时间
                    case 2:
                        memcpy(usefullBuffer, subString, subStringNext - subString);
                        break; // 获取UTC时间
                    case 3:
                        memcpy(Save_Data.latitude, subString, subStringNext - subString);
                        originLat = gpsDataConvert(Save_Data.latitude);
                        break; // 获取纬度信息
                    case 4:
                        memcpy(Save_Data.N_S, subString, subStringNext - subString);
                        break; // 获取N/S
                    case 5:
                        memcpy(Save_Data.longitude, subString, subStringNext - subString);
                        originLng = gpsDataConvert(Save_Data.longitude);
                        break; // 获取经度信息
                    case 6:
                        memcpy(Save_Data.E_W, subString, subStringNext - subString);
                        break; // 获取E/W

                    default:
                        break;
                    }

                    subString = subStringNext;
                    Save_Data.isParseData = true;
                    if (usefullBuffer[0] == 'A')
                        Save_Data.isUsefull = true;
                    else if (usefullBuffer[0] == 'V')
                        Save_Data.isUsefull = false;
                }
                else
                {
                    errorLog(2); // 解析错误
                }
            }
        }
    }
}

// GPS读取函数
void gpsRead()
{
    while (GpsSerial.available())
    {
        gpsRxBuffer[ii++] = GpsSerial.read();
        if (ii == gpsRxBufferLength)
            clrGpsRxBuffer();
    }

    char *GPS_BufferHead;
    char *GPS_BufferTail;
    if ((GPS_BufferHead = strstr(gpsRxBuffer, "$GPRMC,")) != NULL || (GPS_BufferHead = strstr(gpsRxBuffer, "$GNRMC,")) != NULL)
    {
        if (((GPS_BufferTail = strstr(GPS_BufferHead, "\r\n")) != NULL) && (GPS_BufferTail > GPS_BufferHead))
        {
            memcpy(Save_Data.GPS_Buffer, GPS_BufferHead, GPS_BufferTail - GPS_BufferHead);
            Save_Data.isGetData = true;

            clrGpsRxBuffer();
        }
    }
}

// 清空GPS接收缓存区
void clrGpsRxBuffer(void)
{
    memset(gpsRxBuffer, 0, gpsRxBufferLength); // 清空
    ii = 0;
}

// 将度分数据转换成十进制数据
double gpsDataConvert(char *degreeString)
{
    int degrees = static_cast<int>(atof(degreeString) / 100);
    double minutes = atof(degreeString) - degrees * 100;
    return degrees + minutes / 60.0;
}

#endif