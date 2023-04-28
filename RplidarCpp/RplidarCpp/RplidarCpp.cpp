// RplidarCpp.cpp: 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "sl_lidar.h"
#include "sl_lidar_driver.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>

#define _DLLEXPORT __declspec(dllexport)	
#define SupportedScans

using namespace sl;


struct LidarData
{
	bool syncBit;
	float theta;
	float distant;
	UINT quality;
};


class LidarMgr
{
public:
	LidarMgr();
	~LidarMgr();

	const LidarMgr operator=(const LidarMgr&) = delete;
	const LidarMgr(const LidarMgr&) = delete;

public:
	ILidarDriver* lidar_drv;
	IChannel* _channel;
	bool m_isConnected = false;
	sl_lidar_response_device_info_t devinfo;

	bool m_onMotor = false;
	bool m_onScan = false;

	int onConnect(const char* port, sl_u32 baudrate);
	int onConnect(const char* port);
	bool onDisconnect();

	bool checkDeviceHealth(int* errorCode);

	bool startMotor();

	bool startScan();


	bool endMotor();
	bool endScan();

	bool releaseDrv();

	bool grabData(LidarData* ptr) {

		if (!m_onScan) {
			printf("Trying to grab data when we are not scanning !\n");
			return false;
		}
		//Legacy low quality struct
		//rplidar_response_measurement_node_t nodes[360 * 2];
		sl_lidar_response_measurement_node_hq_t nodes[360 * 2];
		size_t   count = _countof(nodes);


		//Legacy low quality call	
		//u_result op_result = lidar_drv->grabScanData(nodes, count);
		sl_result op_result = lidar_drv->grabScanDataHq(nodes, count);
		if (SL_IS_OK(op_result)) {
			lidar_drv->ascendScanData(nodes, count);
			printf("Grabbing data\n");
			for (int pos = 0; pos < (int)count; pos++) {
				//Legacy
				//= nodes[pos].quality /*sync_quality*/ & SL_LIDAR_RESP_MEASUREMENT_EXP_SYNCBIT;
				ptr[pos].syncBit = nodes[pos].flag & SL_LIDAR_RESP_HQ_FLAG_SYNCBIT;
				//Legacy
				//= (nodes[pos].angle_z_q14 /*angle_q6_checkbit*/ >> SL_LIDAR_RESP_MEASUREMENT_ANGLE_SHIFT) / 64.0f;
				ptr[pos].theta = (nodes[pos].angle_z_q14 * 90.f) / 16384.f;
				ptr[pos].distant = nodes[pos].dist_mm_q2 /*distance_q2*/ / 4.0f;
				ptr[pos].quality = nodes[pos].quality /*sync_quality*/ >> SL_LIDAR_RESP_MEASUREMENT_QUALITY_SHIFT;
			}
			return true;
		}
		printf("Failed to grab data, result wasn't o\n ");
		return false;
	}


};

static LidarMgr s_lidarMgr;

ILidarDriver* lidar_drv = nullptr;

LidarMgr::LidarMgr()
{

}
sl_u32 baudrateArray[3] = { 256000,115200,9600 };
int LidarMgr::onConnect(const char* port)
{
	size_t baudRateArraySize = (sizeof(baudrateArray)) / (sizeof(baudrateArray[0]));
	int res = -1;
	for (size_t i = 0; i < baudRateArraySize; ++i)
	{
		auto baudrate = baudrateArray[i];
		auto currentResult = LidarMgr::onConnect(port, baudrate);
		if (currentResult > 0) {
			return currentResult;
		}
		else if (currentResult < -30 && currentResult>-40) {
			printf("lidar_drv failed to fetch device data throught SerialChannel (%s;%d), might be normal if baudrate isn't supported, lower baudrates are automatically tried after this\n", port, baudrate);
		}
		res += (pow(100, i) * currentResult);
	}
	return res + 1;//-21*100
}
/// <summary>
/// 
/// </summary>
/// <param name="port"></param>
/// <param name="baudrate"></param>
/// <returns> If error code starts with -30 : then refer to this table : 
/// #define SL_RESULT_INVALID_DATA           (sl_result)(0x8000 | SL_RESULT_FAIL_BIT)
///OPERATION_FAIL         1 
///OPERATION_TIMEOUT      2 
///OPERATION_STOP         3 
///OPERATION_NOT_SUPPORT  4 
///FORMAT_NOT_SUPPORT     5 
///INSUFFICIENT_MEMORY    6 
/// </returns>
int LidarMgr::onConnect(const char* port, sl_u32 baudrate)
{
	if (m_isConnected) return 0;
	if (lidar_drv == nullptr) {
		//Legacy call, when the enum was in RPLidarDriver class, moved to another one, same behaviour
		//lidar_drv = RPlidarDriver::CreateDriver(/*RPlidarDriver::DRIVER_TYPE_SERIALPORT*/);
		lidar_drv = *createLidarDriver();

		printf("lidar_drv created\n");
	}

	if (lidar_drv == nullptr) {
		return -20;
	}
	_channel = *createSerialPortChannel(port, baudrate);
	sl_result connectionResult = lidar_drv->connect(_channel);
	if (SL_IS_FAIL(connectionResult)) {
		return -21;
	}
	else {
		connectionResult += 5;
	}
	sl_result ans = lidar_drv->getDeviceInfo(devinfo);

	if (SL_IS_FAIL(ans)) {
		auto error = -30 - ((ans | 0x80000000) & 0x0F);
		printf("Failed to get device info with error code :%d, releasing...\n", error);
		releaseDrv();
		return error;
	}
	else {
		printf("lidar_drv connected successfully throught SerialChannel (%s;%d), result is %d, we were able to fetch device informations : \n", port, baudrate, connectionResult);
		printf("SLAMTEC LIDAR S/N: ");
		for (int pos = 0; pos < 16; ++pos) {
			printf("%02X", devinfo.serialnum[pos]);
		}
		printf(", Firmware Ver: %d.%02d, Hardware Rev: %d\n"
			, devinfo.firmware_version >> 8
			, devinfo.firmware_version & 0xFF
			, (int)devinfo.hardware_version);
	}
	sl_lidar_response_device_health_t healthinfo;
	if (SL_IS_OK(lidar_drv->getHealth(healthinfo))) {
		if (healthinfo.status == SL_LIDAR_STATUS_ERROR) {
			fprintf(stderr, "Error, slamtec lidar internal error detected. Please reboot the device to retry.\n");
			printf("Error, slamtec lidar internal error detected. Please reboot the device to retry.\n");
			// enable the following code if you want slamtec lidar to be reboot by software
			lidar_drv->reset();
			delete lidar_drv;
			lidar_drv = NULL;
			return -1000;
		}
	}
	else {
		return (-23 * 1) - (100 * healthinfo.status) - (1000000 * healthinfo.error_code);
	}
	auto code = (connectionResult) * 1 + ((int)healthinfo.status + 5) * 100;
	printf("Connection is ok : %d\n", code);
	m_isConnected = true;
	return code;
}

bool LidarMgr::onDisconnect()
{
	if (m_isConnected) {
		if (lidar_drv != nullptr) {
			printf("lidar_drv disconnetting : scan, motor, then drive\n");
			endScan();
			endMotor();
			m_isConnected = false;
			releaseDrv();
			return true;
		}
		else {
			printf("lidar_drv disconnetting but is already null\n");
		}
	}
	printf("lidar_drv disconnecting but wasn't connected\n");
	return false;
}

bool LidarMgr::startMotor()
{
	if (!m_isConnected) return false;
	if (m_onMotor) return true;

	lidar_drv->setMotorSpeed();
	m_onMotor = true;

	return true;
}

bool LidarMgr::startScan()
{
	printf("Starting scan...\n");
	if (!m_isConnected) {
		printf("But not connected\n");
		return false;
	}
	if (!m_onMotor) {

		printf("But motor not spinning\n");
		return false;
	}
	if (m_onScan) {
		printf("But already scanning, keeping to scan");
		return true;
	}
	//Legacy raw call
	//lidar_drv->startScan();
	LidarScanMode scanMode;
#ifdef SupportedScans
	//You can pick a scan mode from this list like this:
	std::vector<LidarScanMode> scanModes;
	lidar_drv->getAllSupportedScanModes(scanModes);
	scanMode = scanModes[0];
	lidar_drv->startScanExpress(false, scanMode.id);
#else

	//Or you can just use the typical scan mode of RPLIDAR like this:
	lidar_drv->startScan(false, true, 0, &scanMode);
#endif	
	m_onScan = true;
	printf("Is succeed with scan : %d : %s\n", (scanMode.id), scanMode.scan_mode);
	return true;
}

bool LidarMgr::endMotor()
{
	if (!m_isConnected) return false;
	if (!m_onMotor) return true;
	if (m_onScan) {
		endScan();
		printf("Was still scanning when stopping motor, endingScan");
	}
	if (m_onScan) return false;

	lidar_drv->setMotorSpeed(0);
	printf("lidar_drv stop motor\n");
	m_onMotor = false;

	return true;
}

bool LidarMgr::endScan()
{
	if (!m_isConnected) {
		printf("lidar_drv try to stop scan but wasn't connected\n");
		return false;
	}
	if (!m_onScan) {
		printf("lidar_drv try to stop scan but wasn't scanning\n");
		return true;
	}
	lidar_drv->stop();
	printf("lidar_drv stop scan\n");
	m_onScan = false;

	return true;
}

bool LidarMgr::releaseDrv()
{
	//release
	if (lidar_drv != nullptr) {
		lidar_drv->stop();
		delete lidar_drv;
		lidar_drv = nullptr;

		printf("lidar_drv released driver\n");

		if (_channel != nullptr) {
			_channel->flush();
			_channel->close();
			delete _channel;
			_channel = nullptr;
			printf("_channel flushed, closed and released _channel\n");
		}
		return true;
	}

	printf("lidar_drv try to release but was already null\n");
	return true;
}



LidarMgr::~LidarMgr()
{
	printf("In lidarMgr singleton deconstructor :\n ");
	onDisconnect();
	releaseDrv();
}





extern "C" {


	_DLLEXPORT int OnConnect(const char* port) {
		if (port == nullptr) {
			return -30;
		}
		return s_lidarMgr.onConnect(port);
	}
	_DLLEXPORT int OnConnectBaud(const char* port, sl_u32 baudrate) {
		if (port == nullptr) {
			return -30;
		}
		return s_lidarMgr.onConnect(port, baudrate);
	}
	FILE* fp;
	_DLLEXPORT int CancelRedirectPrint() {
		int fd = _fileno(stdout);
		if (_dup2(_fileno(stdout), fd) == -1) {
			std::cerr << "Error restoring stdout to console." << std::endl;
			return -1;
		}

		// Close file
		fclose(fp);
		return 0;
	}
	_DLLEXPORT int RedirectPrintToFile(const char* file_path) {
		// Open file for writing
		if (fopen_s(&fp, file_path, "w") != 0) {
			std::cerr << "Error opening file: " << file_path << std::endl;
			return -10;
		}

		// Redirect stdout to file
		int fd = _fileno(stdout);
		if (_dup2(_fileno(fp), fd) == -1) {
			std::cerr << "Error redirecting stdout to file: " << file_path << std::endl;
			return -1;
		}

		// Set stdout to be unbuffered
		setvbuf(stdout, NULL, _IONBF, 0);
		return 0;
	}

	_DLLEXPORT bool OnDisconnect() {

		return s_lidarMgr.onDisconnect();
	}


	_DLLEXPORT bool StartMotor() {
		return s_lidarMgr.startMotor();
	}

	_DLLEXPORT bool StartScan() {
		return s_lidarMgr.startScan();
	}


	_DLLEXPORT bool EndMotor() {
		return s_lidarMgr.endMotor();
	}

	_DLLEXPORT bool EndScan() {
		return s_lidarMgr.endScan();
	}

	_DLLEXPORT bool ReleaseDrive() {
		return s_lidarMgr.releaseDrv();
	}


	_DLLEXPORT int GetLDataSize() {
		return sizeof(LidarData);
	}

	_DLLEXPORT void GetLDataSample(LidarData* data) {
		data->syncBit = false;
		data->theta = 0.542f;
		data->distant = 100.0;
		data->quality = 23;
	}

	_DLLEXPORT int GrabData(LidarData* data) {

		if (s_lidarMgr.grabData(data)) {
			return 720;
		}
		else
		{
			return 0;
		}
	}

	_DLLEXPORT void GetLDataSampleArray(LidarData* data) {

		data[0].syncBit = false;
		data[0].theta = 0.542f;
		data[0].distant = 100.0;
		data[0].quality = 23;

		data[1].syncBit = true;
		data[1].theta = 2.333f;
		data[1].distant = 50.001f;
		data[1].quality = 7;
	}
}

