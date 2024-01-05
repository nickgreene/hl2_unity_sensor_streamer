#include "pch.h"
#define QOI_IMPLEMENTATION
#include "qoi.h"



#define DBG_ENABLE_VERBOSE_LOGGING 0
#define DBG_ENABLE_INFO_LOGGING 1
#define DBG_ENABLE_ERROR_LOGGING 1

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Networking::Sockets;
using namespace winrt::Windows::Storage::Streams;

//const int VideoCameraStreamer::kImageWidth = 640;
//const wchar_t VideoCameraStreamer::kSensorName[3] = L"PV";
//const long long VideoCameraStreamer::kMinDelta = 2000000 ; // require 200 ms between frames; results in 5 fps


VideoCameraStreamer::VideoCameraStreamer(
    const SpatialCoordinateSystem& coordSystem,
    std::wstring portName)
{
    m_worldCoordSystem = coordSystem;
    m_portName = portName;

    StartServer();
    // m_streamingEnabled = true;


    m_qoi_desc = (qoi_desc *) malloc(sizeof(qoi_desc));
    if (m_qoi_desc == NULL)
    {
        throw new std::bad_alloc();
    }

    m_qoi_desc->width = 0;
    m_qoi_desc->height = 0;
    m_qoi_desc->channels = 3;
    m_qoi_desc->colorspace = 0;


    m_bgr_buf = (uint8_t*)malloc(1952 * 1100 * 4); // max expected frame size for Hololens 2 Video Conferencing Profile
}

IAsyncAction VideoCameraStreamer::StartServer()
{
    try
    {
        m_streamSocketListener.Control().NoDelay(true);
        m_streamSocketListener.Control().QualityOfService(SocketQualityOfService::LowLatency);

        // The ConnectionReceived event is raised when connections are received.
        m_streamSocketListener.ConnectionReceived({ this, &VideoCameraStreamer::OnConnectionReceived });

        // Start listening for incoming TCP connections on the specified port. You can specify any port that's not currently in use.
        // Every protocol typically has a standard port number. For example, HTTP is typically 80, FTP is 20 and 21, etc.
        // For this example, we'll choose an arbitrary port number.
        co_await m_streamSocketListener.BindServiceNameAsync(m_portName);
        //m_streamSocketListener.Control().KeepAlive(true);

#if DBG_ENABLE_INFO_LOGGING       
        wchar_t msgBuffer[200];
        swprintf_s(msgBuffer, L"VideoCameraStreamer::StartServer: Server is listening at %ls \n",
            m_portName.c_str());
        OutputDebugStringW(msgBuffer);
#endif
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"VideoCameraStreamer::StartServer: Failed to open listener with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}

void VideoCameraStreamer::OnConnectionReceived(
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
#if DBG_ENABLE_INFO_LOGGING
        OutputDebugStringW(L"VideoCameraStreamer::OnConnectionReceived: Received connection! \n");
#endif
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        SocketErrorStatus webErrorStatus{ SocketError::GetStatus(ex.to_abi()) };
        winrt::hstring message = webErrorStatus != SocketErrorStatus::Unknown ?
            winrt::to_hstring((int32_t)webErrorStatus) : winrt::to_hstring(ex.to_abi());
        OutputDebugStringW(L"VideoCameraStreamer::StartServer: Failed to establish connection with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }
}


void VideoCameraStreamer::Send(
    MediaFrameReference pFrame,
    long long pTimestamp)
{
    if (m_writeInProgress)
    {
    #if DBG_ENABLE_VERBOSE_LOGGING
            OutputDebugStringW(
                L"VideoCameraStreamer::SendFrame: Write in progress.\n");
    #endif
            return;
    }


#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Received frame for sending!\n");
#endif
    if (!m_streamSocket || !m_writer)
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(
            L"VideoCameraStreamer::SendFrame: No connection.\n");
#endif
        return;
    }


    // grab the frame info
    float fx = pFrame.VideoMediaFrame().CameraIntrinsics().FocalLength().x;
    float fy = pFrame.VideoMediaFrame().CameraIntrinsics().FocalLength().y;

    winrt::Windows::Foundation::Numerics::float4x4 PVtoWorldtransform;
    auto PVtoWorld =
        pFrame.CoordinateSystem().TryGetTransformTo(m_worldCoordSystem);
    if (PVtoWorld)
    {
        PVtoWorldtransform = PVtoWorld.Value();
    }
    else
    {
#if DBG_ENABLE_VERBOSE_LOGGING
        OutputDebugStringW(L"Streamer::SendFrame: Could not locate frame.\n");
#endif
        return;
    }

    // grab the frame data
    SoftwareBitmap softwareBitmap = SoftwareBitmap::Convert(
        pFrame.VideoMediaFrame().SoftwareBitmap(), BitmapPixelFormat::Bgra8);

    int imageWidth = softwareBitmap.PixelWidth();
    int imageHeight = softwareBitmap.PixelHeight();

    int pixelStride = 4;
    int scaleFactor = 1;

    int rowStride = imageWidth * pixelStride;

    // Get bitmap buffer object of the frame
    BitmapBuffer bitmapBuffer = softwareBitmap.LockBuffer(BitmapBufferAccessMode::Read);

    // Get raw pointer to the buffer object
    uint32_t pixelBufferDataLength = 0;
    uint8_t* pixelBufferData;

    auto spMemoryBufferByteAccess{ bitmapBuffer.CreateReference()
        .as<::Windows::Foundation::IMemoryBufferByteAccess>() };

    try
    {
        spMemoryBufferByteAccess->
            GetBuffer(&pixelBufferData, &pixelBufferDataLength);
    }
    catch (winrt::hresult_error const& ex)
    {
#if DBG_ENABLE_ERROR_LOGGING
        winrt::hresult hr = ex.code(); // HRESULT_FROM_WIN32
        winrt::hstring message = ex.message();
        OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Failed to get buffer with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif
    }


    if ( (imageWidth*imageHeight) > (1952 * 1100) )
    {
        OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Unexpected_image_size");
        throw std::bad_alloc();
    }


    m_qoi_desc->width = imageWidth;
    m_qoi_desc->height = imageHeight;
    m_qoi_desc->channels = 3;




    int count = 0;
    for (int row = 0; row < imageHeight; row += scaleFactor)
    {
        for (int col = 0; col < rowStride; col += scaleFactor * pixelStride)
        {
            for (int j = 0; j < pixelStride - 1; j++)
            {
                m_bgr_buf[count] = pixelBufferData[row * rowStride + col + j];
                ++count;
            }
        }
    }

    int out_len = 0;

    // removing compression
    //uint8_t* qoi_buf = (uint8_t*)qoi_encode(m_bgr_buf, m_qoi_desc, &out_len);  // m_qoi_desc has the size of the buffer
    //if (qoi_buf == NULL)
    //{
    //    throw std::bad_alloc();
    //}

                                //std::vector<uint8_t> imageBufferAsVector;

                                //for (int i = 0; i < out_len; i++)
                                //{
                                //    imageBufferAsVector.emplace_back(qoi_buf[i]);
                                //}


    /*const int max_dst_size = LZ4_compressBound(out_len);
    char* compressed_data = (char*)malloc((size_t)max_dst_size)*/;

    //if (compressed_data == NULL)
    //{
    //    OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
    //    throw std::bad_alloc();
    //}

    //const int compressed_data_size = LZ4_compress_default( (char*)qoi_buf, compressed_data, out_len, max_dst_size);

    //free(qoi_buf);

    //if (compressed_data_size <= 0)
    //{
    //    // lz4 error
    //    free(compressed_data);
    //    return;
    //}


    ////second LZ4 pass
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

                            // old from when I removed qoi_encode
                            /*const int max_dst_size = LZ4_compressBound(imageWidth * imageHeight*3);
                            char* compressed_data = (char*)malloc((size_t)max_dst_size);

                            if (compressed_data == NULL)
                            {
                                OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Malloc failed for lz4.\n");
                                throw std::bad_alloc();
                            }

                            const int compressed_data_size = LZ4_compress_default( (char*)m_bgr_buf, compressed_data, imageWidth * imageHeight * 3, max_dst_size);*/




    //std::vector<uint8_t> imageBufferAsVector(compressed_data2, compressed_data2 + compressed_data_size2);
    int bgr_buf_size = imageWidth * imageHeight * 3;
    std::vector<uint8_t> imageBufferAsVector(m_bgr_buf, m_bgr_buf + bgr_buf_size);

  /*  free(compressed_data);
    free(compressed_data2);*/


    m_writeInProgress = true;
    try
    {


        // Write header
        m_writer.WriteUInt64(pTimestamp);
        m_writer.WriteInt32(imageWidth);
        m_writer.WriteInt32(imageHeight);
        m_writer.WriteInt32(pixelStride - 1); // 3
        m_writer.WriteInt32(imageWidth * (pixelStride - 1)); // adapted row stride
        //m_writer.WriteInt32(/*compressed_data_size2*/);
        m_writer.WriteInt32(bgr_buf_size);
        m_writer.WriteSingle(fx);
        m_writer.WriteSingle(fy);

        WriteMatrix4x4(PVtoWorldtransform);


        m_writer.WriteBytes(imageBufferAsVector);
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
        OutputDebugStringW(L"VideoCameraStreamer::SendFrame: Sending failed with ");
        OutputDebugStringW(message.c_str());
        OutputDebugStringW(L"\n");
#endif // DBG_ENABLE_ERROR_LOGGING
    }

    m_writeInProgress = false;

#if DBG_ENABLE_VERBOSE_LOGGING
    OutputDebugStringW(
        L"VideoCameraStreamer::SendFrame: Frame sent!\n");
#endif

}

void VideoCameraStreamer::WriteMatrix4x4(
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
