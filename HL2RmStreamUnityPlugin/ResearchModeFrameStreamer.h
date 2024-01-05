#pragma once
#include "qoi.h"

namespace Depth
{
	enum InvalidationMasks
	{
		Invalid = 0x80,
	};
	static constexpr UINT16 AHAT_INVALID_VALUE = 4090;
}


class ResearchModeFrameStreamer : public IResearchModeFrameSink
{
public:
	ResearchModeFrameStreamer(
		std::wstring portName,
		const GUID& guid,
		const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordSystem);

	void Send(
		std::shared_ptr<IResearchModeSensorFrame> frame,
		ResearchModeSensorType pSensorType);

	//winrt::Windows::Foundation::IAsyncAction SendAndWait(
	//	std::shared_ptr<IResearchModeSensorFrame> frame,
	//	ResearchModeSensorType pSensorType);

	void SendAHAT(
		std::shared_ptr<IResearchModeSensorFrame> frame);

	void SendLongThrow(
		std::shared_ptr<IResearchModeSensorFrame> frame);

	void SendVLC(
		std::shared_ptr<IResearchModeSensorFrame> frame);

public:
	bool isConnected = false;

	//bool m_nextFrameRequested = true;

	//winrt::Windows::Foundation::IAsyncAction ResearchModeFrameStreamer::WaitForRequest2();



private:
	winrt::Windows::Foundation::IAsyncAction StartServer();

	void OnConnectionReceived(
		winrt::Windows::Networking::Sockets::StreamSocketListener /* sender */,
		winrt::Windows::Networking::Sockets::StreamSocketListenerConnectionReceivedEventArgs args);

	void WriteMatrix4x4(
		_In_ winrt::Windows::Foundation::Numerics::float4x4 matrix);

	void SetLocator(const GUID& guid);

	// spatial locators
	winrt::Windows::Perception::Spatial::SpatialLocator m_locator = nullptr;
	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem m_worldCoordSystem = nullptr;

	// socket, listener and writer
	winrt::Windows::Networking::Sockets::StreamSocketListener m_streamSocketListener;
	winrt::Windows::Networking::Sockets::StreamSocket m_streamSocket = nullptr;
	winrt::Windows::Storage::Streams::DataWriter m_writer = nullptr;


	//winrt::Windows::Storage::Streams::DataReader m_reader = nullptr;


	bool m_writeInProgress = false;

	std::wstring m_portName;

	TimeConverter m_converter;



	//void ResearchModeFrameStreamer::WaitForRequest();
	qoi_desc* m_qoi_desc;
	//qoi_desc* m_qoi_desc_depth;
	
	BYTE* depth_combined_buf;

};

