#pragma once
class ResearchModeFrameProcessor
{
public:
	ResearchModeFrameProcessor(
		IResearchModeSensor* pLLSensor,
		HANDLE camConsentGiven,
		ResearchModeSensorConsent* camAccessConsent,
		const unsigned long long minDelta,
		std::shared_ptr<IResearchModeFrameSink> frameSink,
		std::wstring reqPortName);

	~ResearchModeFrameProcessor();

	void Stop();

	void Start();

	bool isRunning = false;

	bool m_sendNextFrame = true;


protected:
	static void CameraUpdateThread(
		ResearchModeFrameProcessor* pProcessor,
		HANDLE camConsentGiven,
		ResearchModeSensorConsent* camAccessConsent);

	static void FrameProcessingThread(
		ResearchModeFrameProcessor* pProcessor);

	bool IsValidTimestamp(
		std::shared_ptr<IResearchModeSensorFrame> pSensorFrame);

	// Mutex to access sensor frame
	std::mutex m_sensorFrameMutex;

	IResearchModeSensor* m_pRMSensor = nullptr;
	std::shared_ptr<IResearchModeSensorFrame> m_pSensorFrame = nullptr;
	std::shared_ptr<IResearchModeFrameSink> m_pFrameSink = nullptr;

	bool m_fExit = false;



	// thread for reading frames
	std::thread m_cameraUpdateThread;
	// thread for processing frames
	std::thread m_processThread;

	UINT64 m_prevTimestamp = 0;
	unsigned long long m_minDelta = 0;
	HANDLE m_camConsentGiven;
	ResearchModeSensorConsent* m_pCamAccessConsent;



private:
	winrt::Windows::Foundation::IAsyncAction StartReqListener();
	winrt::Windows::Networking::Sockets::DatagramSocket  m_datagramSocket = nullptr;

	std::wstring m_reqPortName = nullptr;


	void datagramSocket_MessageReceived(winrt::Windows::Networking::Sockets::DatagramSocket const& /* sender */,
										winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs const& args);



};

