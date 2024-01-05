#include "pch.h"

#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1
#define DBG_ENABLE_ERROR_LOGGING 1

using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Networking::Sockets;



using namespace winrt::Windows::Perception::Spatial;
using namespace std::chrono_literals;

ResearchModeFrameProcessor::ResearchModeFrameProcessor(
    IResearchModeSensor* pLLSensor,
    HANDLE camConsentGiven,
    ResearchModeSensorConsent* camAccessConsent,
    const unsigned long long minDelta,
    std::shared_ptr<IResearchModeFrameSink> frameSink,
    std::wstring reqPortName) :
    m_pRMSensor(pLLSensor),
    m_camConsentGiven(camConsentGiven),
    m_pCamAccessConsent(camAccessConsent),
    m_minDelta(minDelta),
    m_pFrameSink(frameSink),
    m_reqPortName(reqPortName)
{
    m_pRMSensor->AddRef();
    m_pSensorFrame = nullptr;
    m_fExit = false;
 

#if DBG_ENABLE_INFO_LOGGING
    wchar_t msgBuffer[200];
    swprintf_s(msgBuffer, L"ResearchModeFrameProcessor: Created processor for sensor %ls\n",
        pLLSensor->GetFriendlyName());
    OutputDebugStringW(msgBuffer);
#endif




#if DBG_ENABLE_INFO_LOGGING
    swprintf_s(msgBuffer, L"ResearchModeFrameProcessor: attempting to start Req Listener %ls\n",
        pLLSensor->GetFriendlyName());
    OutputDebugStringW(msgBuffer);
#endif
    StartReqListener();

}

ResearchModeFrameProcessor::~ResearchModeFrameProcessor()
{
    m_fExit = true;
    if (m_cameraUpdateThread.joinable())
    {
        m_cameraUpdateThread.join();
    }
    if (m_pRMSensor)
    {
        m_pRMSensor->CloseStream();
        m_pRMSensor->Release();
    }
    if (m_processThread.joinable())
    {
        m_processThread.join();
    }
}

void ResearchModeFrameProcessor::Stop()
{
    m_fExit = true;
    if (m_cameraUpdateThread.joinable())
    {
        m_cameraUpdateThread.join();
    }
    if (m_pRMSensor)
    {
        m_pRMSensor->CloseStream();
    }
    if (m_processThread.joinable())
    {
        m_processThread.join();
    }
    isRunning = false;
}

void ResearchModeFrameProcessor::Start()
{
    m_fExit = false;
    m_cameraUpdateThread = std::thread(CameraUpdateThread, this, m_camConsentGiven, m_pCamAccessConsent);
    m_processThread = std::thread(FrameProcessingThread, this);
    isRunning = true;
}


void ResearchModeFrameProcessor::CameraUpdateThread(
    ResearchModeFrameProcessor* pResearchModeFrameProcessor,
    HANDLE camConsentGiven,
    ResearchModeSensorConsent* camAccessConsent)
{
    HRESULT hr = S_OK;

    // wait for the event to be set and check for the consent provided by the user.
    DWORD waitResult = WaitForSingleObject(camConsentGiven, INFINITE);
    if (waitResult == WAIT_OBJECT_0)
    {
        switch (*camAccessConsent)
        {
        case ResearchModeSensorConsent::Allowed:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is granted. \n");
            break;
        case ResearchModeSensorConsent::DeniedBySystem:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is denied by the system. \n");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::DeniedByUser:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is denied by the user. \n");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::NotDeclaredByApp:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Capability is not declared in the app manifest. \n");
            hr = E_ACCESSDENIED;
            break;
        case ResearchModeSensorConsent::UserPromptRequired:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Capability user prompt required. \n");
            hr = E_ACCESSDENIED;
            break;
        default:
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Access is denied by the system. \n");
            hr = E_ACCESSDENIED;
            break;
        }
    }
    else
    {
        hr = E_UNEXPECTED;
    }

    if (SUCCEEDED(hr))
    {
        // try to open the camera stream
        hr = pResearchModeFrameProcessor->m_pRMSensor->OpenStream();
        if (FAILED(hr))
        {
            pResearchModeFrameProcessor->m_pRMSensor->Release();
            pResearchModeFrameProcessor->m_pRMSensor = nullptr;
#if DBG_ENABLE_ERROR_LOGGING
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Opening the Stream failed.\n");
#endif
        }
#if DBG_ENABLE_INFO_LOGGING
        else
        {
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Starting acquisition loop!\n");
        }
#endif
        // frame acquisition loop
        while (!pResearchModeFrameProcessor->m_fExit && pResearchModeFrameProcessor->m_pRMSensor)
        {

            //OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: acquisition loop.\n");

            hr = S_OK;
            // try to grab the next frame
            IResearchModeSensorFrame* pSensorFrame = nullptr;
            hr = pResearchModeFrameProcessor->m_pRMSensor->GetNextBuffer(&pSensorFrame);

            if (SUCCEEDED(hr))
            {
                std::lock_guard<std::mutex> guard(pResearchModeFrameProcessor->
                    m_sensorFrameMutex);

                std::shared_ptr<IResearchModeSensorFrame> spSensorFrame(pSensorFrame, [](IResearchModeSensorFrame* sf) { sf->Release(); });

                pResearchModeFrameProcessor->m_pSensorFrame = spSensorFrame;
#if DBG_ENABLE_VERBOSE_LOGGING
                OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Updated frame.\n");
#endif
            }
            else
            {
#if DBG_ENABLE_ERROR_LOGGING
                OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Failed getting frame.\n");
#endif
            }

            std::this_thread::sleep_for(1ms); // Required 
        }

        // if thread should exit...
        if (pResearchModeFrameProcessor->m_pRMSensor)
        {
#if DBG_ENABLE_INFO_LOGGING
            OutputDebugStringW(L"ResearchModeFrameProcessor::CameraUpdateThread: Closing the stream.\n");
#endif
            pResearchModeFrameProcessor->m_pRMSensor->CloseStream();
        }
    }
}


void ResearchModeFrameProcessor::FrameProcessingThread(
    ResearchModeFrameProcessor* pProcessor)
{
#if DBG_ENABLE_INFO_LOGGING
    OutputDebugString(L"ResearchModeFrameProcessor::FrameProcessingThread: Starting processing thread.\n");
#endif
    while (true)
    {
        if (pProcessor->m_sendNextFrame)
        {
            if (!pProcessor->m_fExit && pProcessor->m_pFrameSink)
            {
                std::lock_guard<std::mutex> reader_guard(pProcessor->m_sensorFrameMutex);
                if (pProcessor->m_pSensorFrame)
                {
                    if (pProcessor->IsValidTimestamp(pProcessor->m_pSensorFrame))
                    {
                        //OutputDebugString(L"ResearchModeFrameProcessor::FrameProcessingThread: about to send\n");

                        pProcessor->m_pFrameSink->Send(
                            pProcessor->m_pSensorFrame,
                            pProcessor->m_pRMSensor->GetSensorType());
                    }
                }

                pProcessor->m_sendNextFrame = false;
            }

            else
            {
                break;
            }

        }

        std::this_thread::sleep_for(1ms); 
    }
}

bool ResearchModeFrameProcessor::IsValidTimestamp(
    std::shared_ptr<IResearchModeSensorFrame> pSensorFrame)
{
    ResearchModeSensorTimestamp timestamp;
    if (pSensorFrame)
    {
        winrt::check_hresult(pSensorFrame->GetTimeStamp(&timestamp));
        if (m_prevTimestamp == timestamp.HostTicks)
        {
            return false;
        }
        auto delta = timestamp.HostTicks - m_prevTimestamp;
        if (delta < m_minDelta)
        {
            return false;
        }
        m_prevTimestamp = timestamp.HostTicks;
        return true;
    }
    else
    {
        return false;
    }
}




    ////////////////////////////////////////////////////////

    // https://docs.microsoft.com/en-us/windows/uwp/networking/sockets
winrt::Windows::Foundation::IAsyncAction ResearchModeFrameProcessor::StartReqListener()
{
    //OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener - start\n");

    try
    {
        //OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener creating UDP socket\n");

        m_datagramSocket = winrt::Windows::Networking::Sockets::DatagramSocket();


        //OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener adding message received callback\n");
        m_datagramSocket.Control().QualityOfService(SocketQualityOfService::LowLatency);


        // The ConnectionReceived event is raised when connections are received.
        m_datagramSocket.MessageReceived({ this, &ResearchModeFrameProcessor::datagramSocket_MessageReceived });




#if DBG_ENABLE_INFO_LOGGING
        OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener - about to bind...\n");
#endif

        // Start listening for incoming UDP connections on the specified port. You can specify any port that's not currently in use.
        co_await m_datagramSocket.BindServiceNameAsync(m_reqPortName);
 
#if DBG_ENABLE_INFO_LOGGING
        OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener bound to port number\n");
        OutputDebugStringW(m_reqPortName.c_str());
        OutputDebugStringW(L". Listener ready.\n");
#endif
    }
    
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener: Failed to open listener with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}







//winrt::Windows::Foundation::IAsyncAction datagramSocket_MessageReceived(winrt::Windows::Networking::Sockets::DatagramSocket sender, winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs args)
//winrt::Windows::Foundation::IAsyncAction ResearchModeFrameProcessor::datagramSocket_MessageReceived(winrt::Windows::Networking::Sockets::DatagramSocket sender, winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs args)

void ResearchModeFrameProcessor::datagramSocket_MessageReceived(winrt::Windows::Networking::Sockets::DatagramSocket const& /* sender */,
                                    winrt::Windows::Networking::Sockets::DatagramSocketMessageReceivedEventArgs const& args)
{
    //OutputDebugStringW(L"ResearchModeFrameProcessor::datagramSocket_MessageReceived start\n");

    DataReader dataReader{ args.GetDataReader() };
    winrt::hstring request{ dataReader.ReadString(dataReader.UnconsumedBufferLength()) };
   
    if (request == L"1\n")
    {
        //OutputDebugStringW(L"ResearchModeFrameProcessor::datagramSocket_MessageReceived request == 1 = true\n");
        m_sendNextFrame = true;
    }
    else
    {
        //OutputDebugStringW(L"ResearchModeFrameProcessor::datagramSocket_MessageReceived request == 1 = false \n");
    }

#if DBG_ENABLE_INFO_LOGGING
    OutputDebugStringW(L"ResearchModeFrameProcessor::StartReqListener: server received the request:");
    OutputDebugStringW(request.c_str());
    OutputDebugStringW(L"\n");
#endif


    //// Echo the request back as the response.
    //IOutputStream outputStream = co_await sender.GetOutputStreamAsync(args.RemoteAddress(), L"1336");
    //DataWriter dataWriter{ outputStream };
    //dataWriter.WriteString(request);

    //co_await dataWriter.StoreAsync();
    //dataWriter.DetachStream();

}



