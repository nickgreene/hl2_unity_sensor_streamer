#pragma once

#include <shared_mutex>


class VideoCameraFrameProcessor
{
public:
	VideoCameraFrameProcessor(std::wstring reqPortName);

	virtual ~VideoCameraFrameProcessor()
	{
		m_fExit = true;

		if (m_processThread.joinable())
		{
			m_processThread.join();
		}

		// revoke registered delegate
		m_mediaFrameReader.FrameArrived(m_OnFrameArrivedRegistration);
	}

	winrt::Windows::Foundation::IAsyncAction InitializeAsync(
		std::shared_ptr<IVideoFrameSink> pFrameSink,
		long long minDelta = 0);

	winrt::Windows::Foundation::IAsyncAction StartAsync();

	void Stop();

	bool isRunning = false;

	bool m_sendNextFrame = true;


protected:
	void OnFrameArrived(
		const winrt::Windows::Media::Capture::Frames::MediaFrameReader& sender,
		const winrt::Windows::Media::Capture::Frames::MediaFrameArrivedEventArgs& args);

private:

	static void FrameProcesingThread(
		VideoCameraFrameProcessor* pProcessor);

	std::shared_ptr<IVideoFrameSink> m_pFrameSink;

	std::shared_mutex m_frameMutex;
	long long m_latestTimestamp = 0;
	winrt::Windows::Media::Capture::Frames::MediaFrameReference m_latestFrame = nullptr;
	winrt::Windows::Media::Capture::Frames::MediaFrameReader m_mediaFrameReader = nullptr;
	winrt::event_token m_OnFrameArrivedRegistration;

	bool m_fExit = false;

	TimeConverter m_converter;
	std::thread m_processThread;

	long long m_minDelta;

	static const int kImageWidth;
	static const wchar_t kSensorName[3];

	std::wstring m_reqPortName;



	winrt::Windows::Foundation::IAsyncAction StartReqListener();

	winrt::Windows::Networking::Sockets::DatagramSocket  m_datagramSocket = nullptr;
	
	
	void datagramSocket_MessageReceived(winrt::Windows::Networking::Sockets::DatagramSocket const& /* sender */,
		winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs const& args);
};

