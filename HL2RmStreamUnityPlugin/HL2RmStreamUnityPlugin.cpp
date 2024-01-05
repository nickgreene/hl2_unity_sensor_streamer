#include "pch.h"
#include "HL2RmStreamUnityPlugin.h"

#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1

extern "C"
HMODULE LoadLibraryA(
	LPCSTR lpLibFileName
);

static ResearchModeSensorConsent camAccessCheck;
static HANDLE camConsentGiven;
static ResearchModeSensorConsent imuAccessCheck;
static HANDLE imuConsentGiven;

using namespace winrt::Windows::Perception::Spatial;


ResearchModeSensorType depth_type = ResearchModeSensorType::DEPTH_LONG_THROW;



void __stdcall HL2Stream::Initialize()
{
#if DBG_ENABLE_INFO_LOGGING
	OutputDebugStringW(L"HL2Stream::StartStreaming: Initializing...\n");
#endif

	SpatialLocator m_locator = SpatialLocator::GetDefault();
	m_worldOrigin = m_locator.CreateStationaryFrameOfReferenceAtCurrentLocation().CoordinateSystem();

	InitializeResearchModeSensors();
	InitializeResearchModeProcessing();
	auto processOp{ InitializeVideoFrameProcessorAsync() };
	processOp.get();

#if DBG_ENABLE_INFO_LOGGING
	OutputDebugStringW(L"HL2Stream::StartStreaming: Done.\n");
#endif

	StartStreaming();
}

void HL2Stream::StreamingToggle()
{
	if (!isStreaming)
	{
		StartStreaming();
	}
	else
	{
		StopStreaming();
	}
}

void HL2Stream::StartStreaming()
{
#if DBG_ENABLE_INFO_LOGGING
	OutputDebugStringW(L"HL2Stream::StartStreaming: Starting streaming!\n");
#endif
	// start the AHAT processor
	m_pAHATProcessor->Start(); // Comment this line to disable sensor

	// start the LF processor
	m_pLFProcessor->Start(); // Comment this line to disable sensor

	// start the RF processor
	m_pRFProcessor->Start();  // Comment this line to disable sensor

	// start the Video video processor
	m_pVideoFrameProcessor->StartAsync(); // Comment this line to disable sensor


	isStreaming = true;
}

void HL2Stream::StopStreaming()
{
	if (m_pAHATProcessor && m_pAHATProcessor->isRunning)
	{
		m_pAHATProcessor->Stop();
	}
	if (m_pVideoFrameStreamer && m_pVideoFrameProcessor->isRunning)
	{
		m_pVideoFrameProcessor->Stop();
	}
	isStreaming = false;
}


winrt::Windows::Foundation::IAsyncAction HL2Stream::InitializeVideoFrameProcessorAsync()
{
	if (m_videoFrameProcessorOperation &&
		m_videoFrameProcessorOperation.Status() == winrt::Windows::Foundation::AsyncStatus::Completed)
	{
		return;
	}

	// the frame processor
	m_pVideoFrameProcessor = std::make_unique<VideoCameraFrameProcessor>(L"21110");
	m_pVideoFrameStreamer = std::make_shared<VideoCameraStreamer>(m_worldOrigin, L"23940");
	if (!m_pVideoFrameStreamer.get())
	{
		throw winrt::hresult(E_POINTER);
	}
	// initialize the frame processor with a streamer sink
	co_await m_pVideoFrameProcessor->InitializeAsync(m_pVideoFrameStreamer);
}


void HL2Stream::InitializeResearchModeSensors()
{
	HRESULT hr = S_OK;
	size_t sensorCount = 0;
	camConsentGiven = CreateEvent(nullptr, true, false, nullptr);

	// Load research mode library
	HMODULE hrResearchMode = LoadLibraryA("ResearchModeAPI");
	if (hrResearchMode)
	{
#if DBG_ENABLE_VERBOSE_LOGGING
		OutputDebugStringW(L"HL2Stream::InitializeResearchModeSensors: Creating sensor device...\n");
#endif
		// create the research mode sensor device
		typedef HRESULT(__cdecl* PFN_CREATEPROVIDER) (IResearchModeSensorDevice** ppSensorDevice);
		PFN_CREATEPROVIDER pfnCreate = reinterpret_cast<PFN_CREATEPROVIDER>
			(GetProcAddress(hrResearchMode, "CreateResearchModeSensorDevice"));
		if (pfnCreate)
		{
			winrt::check_hresult(pfnCreate(&m_pSensorDevice));
		}
		else
		{
			winrt::check_hresult(E_INVALIDARG);
		}
	}

	// manage consent
	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&m_pSensorDeviceConsent)));
	winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(CamAccessOnComplete));
	winrt::check_hresult(m_pSensorDeviceConsent->RequestIMUAccessAsync(ImuAccessOnComplete));

	m_pSensorDevice->DisableEyeSelection();

	winrt::check_hresult(m_pSensorDevice->GetSensorCount(&sensorCount));
	m_sensorDescriptors.resize(sensorCount);

	winrt::check_hresult(m_pSensorDevice->GetSensorDescriptors(m_sensorDescriptors.data(),
		m_sensorDescriptors.size(), &sensorCount));

	for (const auto& sensorDescriptor : m_sensorDescriptors)
	{
		wchar_t msgBuffer[200];
		if (depth_type == DEPTH_AHAT && sensorDescriptor.sensorType == DEPTH_AHAT)
		{
			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAHATSensor));

			swprintf_s(msgBuffer, L"HL2Stream::InitializeResearchModeSensors: Sensor %ls\n",
				m_pAHATSensor->GetFriendlyName());
			
			OutputDebugStringW(msgBuffer);
		}

		else if (depth_type == DEPTH_LONG_THROW && sensorDescriptor.sensorType == DEPTH_LONG_THROW)
		{
			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pAHATSensor));

			swprintf_s(msgBuffer, L"HL2Stream::InitializeResearchModeSensors: Sensor %ls\n",
				m_pAHATSensor->GetFriendlyName());

			OutputDebugStringW(msgBuffer);
		}

		if (sensorDescriptor.sensorType == LEFT_FRONT)
		{
			// TODO: Add options to Unity Interface for which sensors to activate
			/*if (std::find(m_kenabledsensortypes.begin(), m_kenabledsensortypes.end(), left_front) == m_kenabledsensortypes.end())
			{
				continue;
			}*/

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pLFCameraSensor));

			swprintf_s(msgBuffer, L"HL2Stream::InitializeResearchModeSensors: Sensor %ls\n",
				m_pLFCameraSensor->GetFriendlyName());

			OutputDebugStringW(msgBuffer);
		}


		if (sensorDescriptor.sensorType == RIGHT_FRONT)
		{
			/*if (std::find(m_kenabledsensortypes.begin(), m_kenabledsensortypes.end(), left_front) == m_kenabledsensortypes.end())
			{
				continue;
			}*/

			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pRFCameraSensor));

			swprintf_s(msgBuffer, L"HL2Stream::InitializeResearchModeSensors: Sensor %ls\n",
				m_pRFCameraSensor->GetFriendlyName());

			OutputDebugStringW(msgBuffer);
		}


	}
	OutputDebugStringW(L"HL2Stream::InitializeResearchModeSensors: Done.\n");
	return;
}

void HL2Stream::InitializeResearchModeProcessing()
{
	// Get RigNode id which will be used to initialize
	// the spatial locators for camera readers objects
	GUID guid;
	GetRigNodeId(guid);

	// initialize the AHAT depth streamer
	auto ahatStreamer = std::make_shared<ResearchModeFrameStreamer>(L"23941", guid, m_worldOrigin);
	m_pAHATStreamer = ahatStreamer;

	if (m_pAHATSensor)
	{
		auto processor = std::make_shared<ResearchModeFrameProcessor>(
			m_pAHATSensor, camConsentGiven, &camAccessCheck, 0, m_pAHATStreamer, L"21111");

		m_pAHATProcessor = processor;
	}


	// initialize the VLC Left Front streamer
	auto lfStreamer = std::make_shared<ResearchModeFrameStreamer>(L"23942", guid, m_worldOrigin);
	m_pLFStreamer = lfStreamer;

	if (m_pLFCameraSensor)
	{
		auto processor = std::make_shared<ResearchModeFrameProcessor>(
			m_pLFCameraSensor, camConsentGiven, &camAccessCheck, 0, m_pLFStreamer, L"21112");

		m_pLFProcessor = processor;
	}


	// initialize the VLC Right Front streamer
	auto rfStreamer = std::make_shared<ResearchModeFrameStreamer>(L"23943", guid, m_worldOrigin);
	m_pRFStreamer = rfStreamer;

	if (m_pRFCameraSensor)
	{
		auto processor = std::make_shared<ResearchModeFrameProcessor>(
			m_pRFCameraSensor, camConsentGiven, &camAccessCheck, 0, m_pRFStreamer, L"21113");

		m_pRFProcessor = processor;
	}
}

void HL2Stream::CamAccessOnComplete(ResearchModeSensorConsent consent)
{
	camAccessCheck = consent;
	SetEvent(camConsentGiven);
}

void HL2Stream::ImuAccessOnComplete(ResearchModeSensorConsent consent)
{
	imuAccessCheck = consent;
	SetEvent(imuConsentGiven);
}

void HL2Stream::DisableSensors()
{
#if DBG_ENABLE_VERBOSE_LOGGING
	OutputDebugString(L"HL2Stream::DisableSensors: Disabling sensors...\n");
#endif // DBG_ENABLE_VERBOSE_LOGGING
	if (m_pAHATSensor)
	{
		m_pAHATSensor->Release();
	}
	if (m_pLFCameraSensor)
	{
		m_pLFCameraSensor->Release();
	}
	if (m_pRFCameraSensor)
	{
		m_pRFCameraSensor->Release();
	}
	if (m_pSensorDevice)
	{
		m_pSensorDevice->EnableEyeSelection();
		m_pSensorDevice->Release();
	}
	if (m_pSensorDeviceConsent)
	{
		m_pSensorDeviceConsent->Release();
	}
#if DBG_ENABLE_VERBOSE_LOGGING
	OutputDebugString(L"HL2Stream::DisableSensors: Done.\n");
#endif // DBG_ENABLE_VERBOSE_LOGGING
}

void HL2Stream::GetRigNodeId(GUID& outGuid)
{
	IResearchModeSensorDevicePerception* pSensorDevicePerception;
	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&pSensorDevicePerception)));
	winrt::check_hresult(pSensorDevicePerception->GetRigNodeId(&outGuid));
	pSensorDevicePerception->Release();
}

