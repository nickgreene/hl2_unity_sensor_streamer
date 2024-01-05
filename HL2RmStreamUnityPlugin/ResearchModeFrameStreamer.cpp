#include "pch.h"
#include "qoi.h"

#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1
#define DBG_ENABLE_ERROR_LOGGING 1


using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Perception;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Foundation::Numerics;

ResearchModeFrameStreamer::ResearchModeFrameStreamer(
    std::wstring portName,
    const GUID& guid,
    const winrt::Windows::Perception::Spatial::SpatialCoordinateSystem& coordSystem)
{
    m_portName = portName;
    m_worldCoordSystem = coordSystem;


    m_qoi_desc = (qoi_desc*)malloc(sizeof(qoi_desc));
    if (m_qoi_desc == NULL)
    {
        throw new std::bad_alloc();
    }

    m_qoi_desc->width = 0;
    m_qoi_desc->height = 0;
    m_qoi_desc->channels = 3;
    m_qoi_desc->colorspace = 0;



    //m_qoi_desc_depth = (qoi_desc*)malloc(sizeof(qoi_desc));
    //if (m_qoi_desc_depth == NULL)
    //{
    //    throw new std::bad_alloc();
    //}

    //m_qoi_desc_depth->width = 0;
    //m_qoi_desc_depth->height = 0;
    //m_qoi_desc_depth->channels = 3;
    //m_qoi_desc_depth->colorspace = 0;


    depth_combined_buf = (BYTE*)malloc(sizeof(BYTE)*512*512*2*2); // combined
    if (depth_combined_buf == NULL)
    {
        throw new std::bad_alloc();
    }

    // Get GUID identifying the rigNode to
    // initialize the SpatialLocator
    SetLocator(guid);

    StartServer();
}


// https://docs.microsoft.com/en-us/windows/uwp/networking/sockets
winrt::Windows::Foundation::IAsyncAction ResearchModeFrameStreamer::StartServer()
{
    try
    {
        m_streamSocketListener.Control().NoDelay(true);
        m_streamSocketListener.Control().QualityOfService(SocketQualityOfService::LowLatency);
           
        // The ConnectionReceived event is raised when connections are received.
        m_streamSocketListener.ConnectionReceived({ this, &ResearchModeFrameStreamer::OnConnectionReceived });

        // Start listening for incoming TCP connections on the specified port. You can specify any port that's not currently in use.
        // Every protocol typically has a standard port number. For example, HTTP is typically 80, FTP is 20 and 21, etc.
        // For this example, we'll choose an arbitrary port number.
        co_await m_streamSocketListener.BindServiceNameAsync(m_portName);
#if DBG_ENABLE_INFO_LOGGING
        wchar_t msgBuffer[200];
        swprintf_s(msgBuffer, L"ResearchModeFrameStreamer::StartServer: Server is listening at %ls. \n",
            m_portName.c_str());
        OutputDebugStringW(msgBuffer);
#endif // DBG_ENABLE_INFO_LOGGING

    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"ResearchModeFrameStreamer::StartServer: Failed to open listener with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}

void ResearchModeFrameStreamer::OnConnectionReceived(
    StreamSocketListener /* sender */,
    StreamSocketListenerConnectionReceivedEventArgs args)
{
    try
    {
        m_streamSocket = args.Socket();
        m_writer = DataWriter(args.Socket().OutputStream());
        m_writer.UnicodeEncoding(UnicodeEncoding::Utf8);
        m_writer.ByteOrder(ByteOrder::LittleEndian);

        m_writeInProgress = false;
        isConnected = true;
        //m_streamingEnabled = true;

        //m_reader = DataReader(m_streamSocket.InputStream());


#if DBG_ENABLE_INFO_LOGGING
        wchar_t msgBuffer[200];
        swprintf_s(msgBuffer, L"ResearchModeFrameStreamer::OnConnectionReceived: Received connection at %ls. \n",
            m_portName.c_str());
        OutputDebugStringW(msgBuffer);
#endif // DBG_ENABLE_INFO_LOGGING
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"ResearchModeFrameStreamer::OnConnectionReceived: Failed to establish connection with error ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }

}

void ResearchModeFrameStreamer::Send(
    std::shared_ptr<IResearchModeSensorFrame> frame,
    ResearchModeSensorType pSensorType)
{
    //if (!m_nextFrameRequested)
    //{
    if (pSensorType == ResearchModeSensorType::DEPTH_AHAT)
    {
        SendAHAT(frame);
    }

    else if (pSensorType == ResearchModeSensorType::DEPTH_LONG_THROW)
    {
        SendLongThrow(frame);
    }


    else
    {
        // both left and right ahat
        SendVLC(frame);
    }
    //}
 
    //m_nextFrameRequested = false;

    //OutputDebugStringW(L"ResearchModeFrameStreamer::Send Please wtf i need help here\n");


    //WaitForRequest();

    //co_await WaitForRequest2();


}


//winrt::Windows::Foundation::IAsyncAction ResearchModeFrameStreamer::SendAndWait(
//    std::shared_ptr<IResearchModeSensorFrame> frame,
//    ResearchModeSensorType pSensorType)
//{
//    OutputDebugStringW(L"ResearchModeFrameStreamer::SendAndWait Start\n");
//
//    if (m_nextFrameRequested)
//    {
//        OutputDebugStringW(L"ResearchModeFrameStreamer::SendAndWait 1\n");
//
//        m_nextFrameRequested = false;
//        
//        OutputDebugStringW(L"ResearchModeFrameStreamer::SendAndWait 2\n");
//        Send(frame, pSensorType);
//        
//        OutputDebugStringW(L"ResearchModeFrameStreamer::SendAndWait 3\n");
//        co_await WaitForRequest2();
//        
//        OutputDebugStringW(L"ResearchModeFrameStreamer::SendAndWait 4\n");
//        m_nextFrameRequested = true;
//
//        OutputDebugStringW(L"ResearchModeFrameStreamer::SendAndWait End\n");
//
//    }
//
//}




void ResearchModeFrameStreamer::SendAHAT(
    std::shared_ptr<IResearchModeSensorFrame> frame)
{
    if (m_writeInProgress)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Write already in progress.\n");
#endif
        return;
    }



    #if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Received frame for sending!\n");
#endif

    if (!m_streamSocket || !m_writer)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: No connection.\n");
#endif
        return;
    }

    // grab the frame info
    ResearchModeSensorTimestamp rmTimestamp;

    // there is a lot of repeated work 
    winrt::check_hresult(frame->GetTimeStamp(&rmTimestamp));
    
    

    auto prevTimestamp = rmTimestamp.HostTicks;

    auto timestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(HundredsOfNanoseconds(checkAndConvertUnsigned(prevTimestamp)));
    auto location = m_locator.TryLocateAtTimestamp(timestamp, m_worldCoordSystem);
    if (!location)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Can't locate frame.\n");
#endif
        return;
    }
    const float4x4 rig2worldTransform = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());
    auto absoluteTimestamp = m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds((long long)prevTimestamp)).count();

    // grab the frame data
    ResearchModeSensorResolution resolution;

    IResearchModeSensorDepthFrame* pDepthFrame = nullptr;

    size_t outBufferCountDepth;
    size_t outBufferCountAb;
    const UINT16* pDepth = nullptr;
    const UINT16* pAbImage = nullptr;

    // invalidation value for AHAT 
    USHORT maxValue = 4090;

    frame->GetResolution(&resolution);
    HRESULT hr = frame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));

    if (!pDepthFrame || !SUCCEEDED(hr))
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Failed to grab depth frame.\n");
#endif
        return;
    }

    std::shared_ptr<IResearchModeSensorDepthFrame> spDepthFrame(pDepthFrame, [](IResearchModeSensorDepthFrame* sf) { sf->Release(); });

    int imageWidth = resolution.Width;
    int imageHeight = resolution.Height;
    int pixelStride = resolution.BytesPerPixel;

    int rowStride = imageWidth * pixelStride;

    hr = spDepthFrame->GetBuffer(&pDepth, &outBufferCountDepth);

    if (!SUCCEEDED(hr))
    {
        return;
    }

    /*std::vector<BYTE> depthByteData;
    depthByteData.reserve( (outBufferCountDepth) * sizeof(UINT16));*/


    // validate depth & append to vector
    for (size_t i = 0; i < outBufferCountDepth; ++i)
    {
        // use a different invalidation condition for Long Throw and AHAT 
        const bool invalid = (pDepth[i] >= maxValue);
        UINT16 d;
        if (invalid)
        {
            d = 0;
        }
        else
        {
            d = pDepth[i];
        }
        depth_combined_buf[i * 2] = (BYTE)(d >> 8);
        depth_combined_buf[i * 2 + 1] = (BYTE)(d);
        //depthByteData.push_back((BYTE)(d >> 8));
        //depthByteData.push_back((BYTE)d);

    }



    hr = spDepthFrame->GetAbDepthBuffer(&pAbImage, &outBufferCountAb);
    if (!SUCCEEDED(hr))
    {
        return;
    }

    //std::vector<BYTE> AbByteData;
    //AbByteData.reserve((outBufferCountAb) * sizeof(UINT16));

    // ab Image
    for (size_t i = 0; i < (outBufferCountAb); ++i)
    {
        // use a different invalidation condition for Long Throw and AHAT 
        const bool invalid = (pAbImage[i] >= maxValue);
        UINT16 d;
        if (invalid)
        {
            d = 0;
        }
        else
        {
            d = pAbImage[i];
            //depth_combined_buf[outBufferCountDepth+i] = pAbImage[i];
        }
        //AbByteData.push_back((BYTE)(d >> 8));
        //AbByteData.push_back((BYTE)d);
        depth_combined_buf[outBufferCountDepth * 2 + i * 2] = (BYTE)(d >> 8);
        depth_combined_buf[outBufferCountDepth * 2 + i * 2 + 1] = (BYTE)(d);

    }

    m_qoi_desc->width = 512;    // trick for encoding a greyscale image
    m_qoi_desc->height = 512;
    m_qoi_desc->channels = 4; // already initialized to 4

    int out_len = 0;

    //uint8_t* qoi_buf = (uint8_t*)qoi_encode(depth_combined_buf, m_qoi_desc, &out_len);  // m_qoi_desc has the size of the buffer
    //if (qoi_buf == NULL)
    //{
    //    throw std::bad_alloc();
    //}

    //const int max_dst_size = LZ4_compressBound(out_len);
    //char* compressed_data = (char*)malloc((size_t)max_dst_size);

    //if (compressed_data == NULL)
    //{
    //    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size = LZ4_compress_default((char*)qoi_buf, compressed_data, out_len, max_dst_size);


    //free(qoi_buf);

    //if (compressed_data_size <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    return;
    //}


    //const int max_dst_size2 = LZ4_compressBound(compressed_data_size);
    //char* compressed_data2 = (char*)malloc((size_t)max_dst_size2);

    //if (compressed_data2 == NULL)
    //{
    //    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size2 = LZ4_compress_default(compressed_data, compressed_data2, compressed_data_size, max_dst_size2);

    //if (compressed_data_size2 <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    free(compressed_data2);
    //    return;
    //}


    //std::vector<BYTE> DepthCombinedByteBuffer(compressed_data2, compressed_data2 + compressed_data_size2); // TODO: does this make a copy?

    int combined_buf_size = imageHeight * imageHeight * pixelStride * 2; //use bytes per pixel instead of hard coding
    std::vector<BYTE> DepthCombinedByteBuffer(depth_combined_buf, depth_combined_buf + combined_buf_size); // TODO: does this make a copy?

    //free(compressed_data);
    //free(compressed_data2);



    m_writeInProgress = true;

    try
    {
        // Write header
        m_writer.WriteUInt64(absoluteTimestamp);
        m_writer.WriteInt32(imageWidth);
        m_writer.WriteInt32(imageHeight);
        m_writer.WriteInt32(pixelStride);
        m_writer.WriteInt32(rowStride);
        //m_writer.WriteInt32(compressed_data_size2);
        m_writer.WriteInt32(combined_buf_size);


        WriteMatrix4x4(rig2worldTransform);

        m_writer.WriteBytes(DepthCombinedByteBuffer);
        //m_writer.WriteBytes(AbByteData);

#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Trying to store writer...\n");
#endif
        m_writer.StoreAsync();
    }
    catch (winrt::hresult_error const& ex)
    {
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        if (webErrorStatus == SocketErrorStatus::ConnectionResetByPeer)
        {
            // the client disconnected!
            m_writer == nullptr;
            m_streamSocket == nullptr;
            m_writeInProgress = false;
        }
#if DBG_ENABLE_ERROR_LOGGING
        winrt::hstring message = ex.message();
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Sending failed with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif // DBG_ENABLE_ERROR_LOGGING
    }

    m_writeInProgress = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Frame sent!\n");
#endif

    // Release() not needed because the shared pointer spDepthFrame calls it when it goes out of scope
    //if (pDepthFrame)
    //    pDepthFrame->Release();


}



void ResearchModeFrameStreamer::SendLongThrow(
    std::shared_ptr<IResearchModeSensorFrame> frame)
{
    if (m_writeInProgress)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Write already in progress.\n");
#endif
        return;
    }



#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Received frame for sending!\n");
#endif

    if (!m_streamSocket || !m_writer)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: No connection.\n");
#endif
        return;
    }

    // grab the frame info
    ResearchModeSensorTimestamp rmTimestamp;

    // there is a lot of repeated work 
    winrt::check_hresult(frame->GetTimeStamp(&rmTimestamp));



    auto prevTimestamp = rmTimestamp.HostTicks;

    auto timestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(HundredsOfNanoseconds(checkAndConvertUnsigned(prevTimestamp)));
    auto location = m_locator.TryLocateAtTimestamp(timestamp, m_worldCoordSystem);
    if (!location)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Can't locate frame.\n");
#endif
        return;
    }
    const float4x4 rig2worldTransform = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());
    auto absoluteTimestamp = m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds((long long)prevTimestamp)).count();

    // grab the frame data
    ResearchModeSensorResolution resolution;

    IResearchModeSensorDepthFrame* pDepthFrame = nullptr;

    const UINT16* pDepth = nullptr;
    size_t outBufferCountDepth;

    const UINT16* pAbImage = nullptr;
    size_t outBufferCountAb;

    const BYTE* pSigma = nullptr;
    size_t outSigmaBufferCount = 0;

    // invalidation value for AHAT 
    USHORT maxValue = 4090;

    frame->GetResolution(&resolution);
    HRESULT hr = frame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));

    if (!pDepthFrame || !SUCCEEDED(hr))
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Failed to grab depth frame.\n");
#endif
        return;
    }

    std::shared_ptr<IResearchModeSensorDepthFrame> spDepthFrame(pDepthFrame, [](IResearchModeSensorDepthFrame* sf) { sf->Release(); });

    int imageWidth = resolution.Width;
    int imageHeight = resolution.Height;
    int pixelStride = resolution.BytesPerPixel;

    int rowStride = imageWidth * pixelStride;

    hr = spDepthFrame->GetBuffer(&pDepth, &outBufferCountDepth);

    if (!SUCCEEDED(hr))
    {
        return;
    }

    hr = pDepthFrame->GetSigmaBuffer(&pSigma, &outSigmaBufferCount);

    if (!SUCCEEDED(hr))
    {
        return;
    }

    /*std::vector<BYTE> depthByteData;
    depthByteData.reserve( (outBufferCountDepth) * sizeof(UINT16));*/


    // validate depth & append to vector
    for (size_t i = 0; i < outBufferCountDepth; ++i)
    {
        //// use a different invalidation condition for Long Throw and AHAT 
        const bool invalid = (pSigma[i] & Depth::InvalidationMasks::Invalid) > 0;
        UINT16 d;
        if (invalid)
        {
            d = 0;
        }
        else
        {
            d = pDepth[i];
        }

        depth_combined_buf[i * 2] = (BYTE)(d);
        depth_combined_buf[i * 2 + 1] = (BYTE)(d >> 8);
        //depthByteData.push_back((BYTE)(d >> 8));
        //depthByteData.push_back((BYTE)d);

    }



    hr = spDepthFrame->GetAbDepthBuffer(&pAbImage, &outBufferCountAb);
    if (!SUCCEEDED(hr))
    {
        return;
    }

    //std::vector<BYTE> AbByteData;
    //AbByteData.reserve((outBufferCountAb) * sizeof(UINT16));

    // ab Image
    for (size_t i = 0; i < (outBufferCountAb); ++i)
    {
        // use a different invalidation condition for Long Throw and AHAT 
        //const bool invalid = (pAbImage[i] >= maxValue);
        UINT16 d;
      /*  if (invalid)
        {
            d = 0;
        }
        else
        {*/
            d = pAbImage[i];
            //depth_combined_buf[outBufferCountDepth+i] = pAbImage[i];
        //}
        //AbByteData.push_back((BYTE)(d >> 8));
        //AbByteData.push_back((BYTE)d);
        depth_combined_buf[outBufferCountDepth * 2 + i * 2] = (BYTE)(d);
        depth_combined_buf[outBufferCountDepth * 2 + i * 2 + 1] = (BYTE)(d >> 8);

    }

    m_qoi_desc->width = 320;    // trick for encoding a greyscale image
    m_qoi_desc->height = 288;
    m_qoi_desc->channels = 4; // already initialized to 4

    int out_len = 0;

    //uint8_t* qoi_buf = (uint8_t*)qoi_encode(depth_combined_buf, m_qoi_desc, &out_len);  // m_qoi_desc has the size of the buffer
    //if (qoi_buf == NULL)
    //{
    //    throw std::bad_alloc();
    //}

    //const int max_dst_size = LZ4_compressBound(out_len);
    //char* compressed_data = (char*)malloc((size_t)max_dst_size);

    //if (compressed_data == NULL)
    //{
    //    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size = LZ4_compress_default((char*)qoi_buf, compressed_data, out_len, max_dst_size);


    //free(qoi_buf);

    //if (compressed_data_size <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    return;
    //}


    //const int max_dst_size2 = LZ4_compressBound(compressed_data_size);
    //char* compressed_data2 = (char*)malloc((size_t)max_dst_size2);

    //if (compressed_data2 == NULL)
    //{
    //    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size2 = LZ4_compress_default(compressed_data, compressed_data2, compressed_data_size, max_dst_size2);

    //if (compressed_data_size2 <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    free(compressed_data2);
    //    return;
    //}


    //std::vector<BYTE> DepthCombinedByteBuffer(compressed_data2, compressed_data2 + compressed_data_size2); // TODO: does this make a copy?

    int depth_combined_size = imageHeight * imageWidth * pixelStride * 2;
    std::vector<BYTE> DepthCombinedByteBuffer(depth_combined_buf, depth_combined_buf + depth_combined_size); // TODO: does this make a copy?

    /*free(compressed_data);
    free(compressed_data2);*/



    m_writeInProgress = true;

    try
    {
        // Write header
        m_writer.WriteUInt64(absoluteTimestamp);
        m_writer.WriteInt32(imageWidth);
        m_writer.WriteInt32(imageHeight);
        m_writer.WriteInt32(pixelStride);
        m_writer.WriteInt32(rowStride);
        //m_writer.WriteInt32(compressed_data_size2);
        m_writer.WriteInt32(depth_combined_size);


        WriteMatrix4x4(rig2worldTransform);

        m_writer.WriteBytes(DepthCombinedByteBuffer);
        //m_writer.WriteBytes(AbByteData);

#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Trying to store writer...\n");
#endif
        m_writer.StoreAsync();
    }
    catch (winrt::hresult_error const& ex)
    {
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        if (webErrorStatus == SocketErrorStatus::ConnectionResetByPeer)
        {
            // the client disconnected!
            m_writer == nullptr;
            m_streamSocket == nullptr;
            m_writeInProgress = false;
        }
#if DBG_ENABLE_ERROR_LOGGING
        winrt::hstring message = ex.message();
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Sending failed with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif // DBG_ENABLE_ERROR_LOGGING
    }

    m_writeInProgress = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Frame sent!\n");
#endif

    // Release() not needed because the shared pointer spDepthFrame calls it when it goes out of scope
    //if (pDepthFrame)
    //    pDepthFrame->Release();


}




void ResearchModeFrameStreamer::SendVLC(
    std::shared_ptr<IResearchModeSensorFrame> frame)
{
#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Received frame for sending!\n");
#endif

    if (!m_streamSocket || !m_writer)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: No connection.\n");
#endif
        return;
    }

    // grab the frame info
    ResearchModeSensorTimestamp rmTimestamp;

    // there is a lot of repeated work 
    winrt::check_hresult(frame->GetTimeStamp(&rmTimestamp));


    auto prevTimestamp = rmTimestamp.HostTicks;

    auto timestamp = PerceptionTimestampHelper::FromSystemRelativeTargetTime(HundredsOfNanoseconds(checkAndConvertUnsigned(prevTimestamp)));
    auto location = m_locator.TryLocateAtTimestamp(timestamp, m_worldCoordSystem);
    if (!location)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Can't locate frame.\n");
#endif
        return;
    }
    const float4x4 rig2worldTransform = make_float4x4_from_quaternion(location.Orientation()) * make_float4x4_translation(location.Position());
    auto absoluteTimestamp = m_converter.RelativeTicksToAbsoluteTicks(HundredsOfNanoseconds((long long)prevTimestamp)).count();

    // grab the frame data
    ResearchModeSensorResolution resolution;

    IResearchModeSensorVLCFrame* pVLCFrame = nullptr;

    frame->GetResolution(&resolution);
    HRESULT hr = frame->QueryInterface(IID_PPV_ARGS(&pVLCFrame));

    if (!pVLCFrame || !SUCCEEDED(hr))
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::Send: Failed to grab depth frame.\n");
#endif
        return;
    }
    
    // TODO: Why shared pointers???
    std::shared_ptr<IResearchModeSensorVLCFrame> spVLCFrame(pVLCFrame, [](IResearchModeSensorVLCFrame* sf) { sf->Release(); });

    // Convert the software bitmap to raw bytes    
    size_t outBufferCount = 0;
    const BYTE* pImage = nullptr;


    winrt::check_hresult(spVLCFrame->GetBuffer(&pImage, &outBufferCount));

    m_qoi_desc->width = 320;    // trick for encoding a greyscale image
    m_qoi_desc->height = 320;
    m_qoi_desc->channels = 3; // already initialized to 4

    int out_len = 0;

    //uint8_t* qoi_buf = (uint8_t*)qoi_encode(pImage, m_qoi_desc, &out_len);  // m_qoi_desc has the size of the buffer
    //if (qoi_buf == NULL)
    //{
    //    throw std::bad_alloc();
    //}

                        //std::vector<uint8_t> imageBufferAsVector;


                        //for (int i = 0; i < out_len; i++)
                        //{
                        //    imageBufferAsVector.emplace_back(qoi_buf[i]);
                        //}



    //const int max_dst_size = LZ4_compressBound(out_len);
    //char* compressed_data = (char*)malloc((size_t)max_dst_size);

    //if (compressed_data == NULL)
    //{
    //    OutputDebugStringW(L"ResearchModeCameraStreamer::SendVLC: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size = LZ4_compress_default((char*)qoi_buf, compressed_data, out_len, max_dst_size);

    //free(qoi_buf);

    //if (compressed_data_size <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    return;
    //}

    //const int max_dst_size2 = LZ4_compressBound(compressed_data_size);
    //char* compressed_data2 = (char*)malloc((size_t)max_dst_size2);

    //if (compressed_data2 == NULL)
    //{
    //    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size2 = LZ4_compress_default(compressed_data, compressed_data2, compressed_data_size, max_dst_size2);

    //if (compressed_data_size2 <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    free(compressed_data2);
    //    return;
    //}
    int imageWidth = resolution.Width;
    int imageHeight = resolution.Height;
    int pixelStride = resolution.BytesPerPixel;

    int rowStride = imageWidth * pixelStride;


    //std::vector<BYTE> VLCByteBuffer(compressed_data2, compressed_data2 + compressed_data_size2); // TODO: does this make a copy?
    int vlc_image_size = imageWidth * imageHeight * pixelStride;
    std::vector<BYTE> VLCByteBuffer(pImage, pImage + vlc_image_size); // TODO: does this make a copy?

    //free(compressed_data/*);
    //free(compressed_data2);*/


    //std::array<BYTE, outBufferCount> pixelBuffer = { p_image }



    if (m_writeInProgress)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Write already in progress.\n");
#endif
        return;
    }

    m_writeInProgress = true;

    try
    {
        // Write header
        m_writer.WriteUInt64(absoluteTimestamp);
        m_writer.WriteInt32(imageWidth);
        m_writer.WriteInt32(imageHeight);
        m_writer.WriteInt32(pixelStride);
        m_writer.WriteInt32(rowStride);
        //m_writer.WriteInt32(compressed_data_size2);

        m_writer.WriteInt32(vlc_image_size);

        WriteMatrix4x4(rig2worldTransform);

        m_writer.WriteBytes(VLCByteBuffer);

#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Trying to store writer...\n");
#endif
        m_writer.StoreAsync();
    }
    catch (winrt::hresult_error const& ex)
    {
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        if (webErrorStatus == SocketErrorStatus::ConnectionResetByPeer)
        {
            // the client disconnected!
            m_writer == nullptr;
            m_streamSocket == nullptr;
            m_writeInProgress = false;
        }
#if DBG_ENABLE_ERROR_LOGGING
        winrt::hstring message = ex.message();
        OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Sending failed with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif // DBG_ENABLE_ERROR_LOGGING
    }

    m_writeInProgress = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"ResearchModeFrameStreamer::SendFrame: Frame sent!\n");
#endif
}



//void ResearchModeFrameStreamer::WaitForRequest()
//{
//    #if DBG_ENABLE_INFO_LOGGING
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: Start\n");
//    #endif // DBG_ENABLE_INFO_LOGGING
//
//    try
//    {
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1\n");
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1.1\n");
//        if (m_streamSocket && m_writer)
//        {
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1.25\n");
//        }
//
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1.5\n");
//
//        //if (m_streamSocket && m_writer && m_reader)
//        if (m_streamSocket && m_writer)
//        {
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 2\n");
//
//            // Read data from the echo server.
//            DataReader dataReader{ m_streamSocket.InputStream() };
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 3\n");
//
//            winrt::hstring response{ dataReader.ReadString(5) };
//                
//            //std::string res_string = winrt::to_string(response);
//            
//            
//            //wchar_t msgBuffer2[200];
//            //swprintf_s(msgBuffer2, L"ResearchModeFrameStreamer::WaitForRequest: nextFrameRequest received: %u \n",
//            //    bytesLoaded);
//            //OutputDebugStringW(msgBuffer2);
//
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 4\n");
//
//
//
//
//
//
//
//            //unsigned int bytesLoaded = co_await m_reader.LoadAsync(sizeof(bool));
//
//
//            //bool nextFrameRequest = m_reader.ReadBoolean();
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 5\n");
//
//#if DBG_ENABLE_INFO_LOGGING
//            wchar_t msgBuffer[200];
//            swprintf_s(msgBuffer, L"ResearchModeFrameStreamer::WaitForRequest: nextFrameRequest received: %ls \n",
//                response);
//            OutputDebugStringW(msgBuffer);
//#endif // DBG_ENABLE_INFO_LOGGING
//        }
//    }
//
//    catch (winrt::hresult_error const& ex)
//    {
//        #if DBG_ENABLE_ERROR_LOGGING
//            SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
//            winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
//                winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: Failed to open reader with ");
//            OutputDebugStringW(message.c_str());
//            OutputDebugStringW(L"\n");
//        #endif // DBG_ENABLE_ERROR_LOGGING
//    }
//
//
//#if DBG_ENABLE_INFO_LOGGING
//    OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: End\n");
//#endif // DBG_ENABLE_INFO_LOGGING
//
//}
//
//
//
//
//
//
//winrt::Windows::Foundation::IAsyncAction ResearchModeFrameStreamer::WaitForRequest2()
//{
//#if DBG_ENABLE_INFO_LOGGING
//    OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: Start\n");
//#endif // DBG_ENABLE_INFO_LOGGING
//
//    try
//    {
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1\n");
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1.1\n");
//        if (m_streamSocket && m_writer)
//        {
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1.25\n");
//        }
//
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 1.5\n");
//
//        //if (m_streamSocket && m_writer && m_reader)
//        if (m_streamSocket && m_writer)
//        {
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 2\n");
//
//            // Read data from the echo server.
//            DataReader dataReader{ m_streamSocket.InputStream() };
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 3\n");
//
//            unsigned int bytesLoaded = co_await dataReader.LoadAsync(6);
//            
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 3.5\n");
//
//
//            wchar_t msgBuffer2[200];
//            swprintf_s(msgBuffer2, L"ResearchModeFrameStreamer::WaitForRequest: bytesLoaded: %u \n",
//                bytesLoaded);
//            OutputDebugStringW(msgBuffer2);
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 3.75\n");
//
//            winrt::hstring response{ dataReader.ReadString(5) };
//
//            //std::string res_string = winrt::to_string(response);
//
//
//
//
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 4\n");
//
//
//
//
//
//
//
//            //unsigned int bytesLoaded = co_await m_reader.LoadAsync(sizeof(bool));
//
//
//            //bool nextFrameRequest = m_reader.ReadBoolean();
//
//            OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: 5\n");
//
//#if DBG_ENABLE_INFO_LOGGING
//            wchar_t msgBuffer[200];
//            swprintf_s(msgBuffer, L"ResearchModeFrameStreamer::WaitForRequest: nextFrameRequest received: %ls \n",
//                response);
//            OutputDebugStringW(msgBuffer);
//#endif // DBG_ENABLE_INFO_LOGGING
//        }
//    }
//
//    catch (winrt::hresult_error const& ex)
//    {
//#if DBG_ENABLE_ERROR_LOGGING
//        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
//        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
//            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
//        OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: Failed to open reader with ");
//        OutputDebugStringW(message.c_str());
//        OutputDebugStringW(L"\n");
//#endif // DBG_ENABLE_ERROR_LOGGING
//    }
//
//
//#if DBG_ENABLE_INFO_LOGGING
//    OutputDebugStringW(L"ResearchModeFrameStreamer::WaitForRequest: End\n");
//#endif // DBG_ENABLE_INFO_LOGGING
//
//
//}
//
//
//
//
//






void ResearchModeFrameStreamer::WriteMatrix4x4(
    _In_ winrt::Windows::Foundation::Numerics::float4x4 matrix)
{
    m_writer.WriteSingle(matrix.m11);
    m_writer.WriteSingle(matrix.m12);
    m_writer.WriteSingle(matrix.m13);
    m_writer.WriteSingle(matrix.m14);

    m_writer.WriteSingle(matrix.m21);
    m_writer.WriteSingle(matrix.m22);
    m_writer.WriteSingle(matrix.m23);
    m_writer.WriteSingle(matrix.m24);

    m_writer.WriteSingle(matrix.m31);
    m_writer.WriteSingle(matrix.m32);
    m_writer.WriteSingle(matrix.m33);
    m_writer.WriteSingle(matrix.m34);

    m_writer.WriteSingle(matrix.m41);
    m_writer.WriteSingle(matrix.m42);
    m_writer.WriteSingle(matrix.m43);
    m_writer.WriteSingle(matrix.m44);
}

void ResearchModeFrameStreamer::SetLocator(const GUID& guid)
{
    m_locator = Preview::SpatialGraphInteropPreview::CreateLocatorForNode(guid);
}
