import socket
import struct
import abc
import threading
from collections import namedtuple
import numpy as np
import time
import select
import _thread
import qoi
import lz4.block

###############################################################################
# USER ADJUSTABLE PARAMETERS

VIDEO_STREAM_PORT = 23940
DEPTH_STREAM_PORT = 23941
LEFT_FRONT_STREAM_PORT = 23942
RIGHT_FRONT_STREAM_PORT = 23943

VIDEO_UDP_PORT = 21110
DEPTH_UDP_PORT = 21111
LEFT_FRONT_UDP_PORT = 21112
RIGHT_FRONT_UDP_PORT = 21113

VIDEO_REQUEST_TIMEOUT = .1
DEPTH_REQUEST_TIMEOUT = .1
VLC_REQUEST_TIMEOUT = .1

FPS_PRINT_INTERVAL = 10

SOCKET_RESTART_TIMEOUT = 3


###############################################################################

np.warnings.filterwarnings('ignore')

# Definitions
# Protocol Header Format
# see https://docs.python.org/2/library/struct.html#format-characters
VIDEO_STREAM_HEADER_FORMAT = "@qIIIII18f"

VIDEO_FRAME_STREAM_HEADER = namedtuple(
    'SensorFrameStreamHeader',
    'Timestamp ImageWidth ImageHeight PixelStride RowStride BufLen fx fy '
    'PVtoWorldtransformM11 PVtoWorldtransformM12 PVtoWorldtransformM13 PVtoWorldtransformM14 '
    'PVtoWorldtransformM21 PVtoWorldtransformM22 PVtoWorldtransformM23 PVtoWorldtransformM24 '
    'PVtoWorldtransformM31 PVtoWorldtransformM32 PVtoWorldtransformM33 PVtoWorldtransformM34 '
    'PVtoWorldtransformM41 PVtoWorldtransformM42 PVtoWorldtransformM43 PVtoWorldtransformM44 '
)

RM_STREAM_HEADER_FORMAT = "@qIIIII16f"

RM_FRAME_STREAM_HEADER = namedtuple(
    'SensorFrameStreamHeader',
    'Timestamp ImageWidth ImageHeight PixelStride RowStride BufLen '
    'rig2worldTransformM11 rig2worldTransformM12 rig2worldTransformM13 rig2worldTransformM14 '
    'rig2worldTransformM21 rig2worldTransformM22 rig2worldTransformM23 rig2worldTransformM24 '
    'rig2worldTransformM31 rig2worldTransformM32 rig2worldTransformM33 rig2worldTransformM34 '
    'rig2worldTransformM41 rig2worldTransformM42 rig2worldTransformM43 rig2worldTransformM44 '
)


class FrameReceiverThread(threading.Thread):
    def __init__(self, host, port, udp_port, header_format, header_data, req_resend_timeout, sensor_name="NoSensorName"):
        super(FrameReceiverThread, self).__init__()
        self.header_size = struct.calcsize(header_format)
        self.header_format = header_format
        self.header_data = header_data
        self.host = host
        self.port = port
        self.udp_port = udp_port

        self.latest_frame = None
        self.latest_header = None
        self.socket = None
        self.udp_socket = None

        self.req_resend_timeout = req_resend_timeout
        self.sensor_name = sensor_name

        self.lock = threading.Lock()

        self.should_stop = False


        self.motion_detector = None
        self.no_motion = False

        self.last_frame_req_timestamp = time.time()


    def recvall(self, size, timeout=None):
        received_any = False # set to True if at least 1 byte received
        msg = bytearray()    #       

        # Check with select() for data on the socket.
        # If select times out
        # before any data is received None is returned. If some data is received
        # at all, then it is assumed that in total `size` bytes are coming. This
        # will keep reading until the expected number of bytes come in
        while len(msg) < size:
            if (timeout is None) or (received_any):
                # if there is a timeout or some bytes have been read, select
                # will wait indefinitely since more bytes should be coming
                read_sockets, _, _ = select.select([self.socket], [], [])

            else:
                # if there is a timeout, and there is nothing to read from
                # select(), return none
                read_sockets, _, _ = select.select([self.socket], [], [], timeout)

                if (len(read_sockets) == 0) and (not received_any):
                    
                    return None

            for s in read_sockets:
                if s == self.socket:
                    part = self.socket.recv(size - len(msg)) # blocks until data

                    if part != '':
                        received_any = True

                        msg += part
                    else:
                        print("ERROR: empty recv part")
                        # interrupts main thread, and causes the program to exit
                        _thread.interrupt_main()

        return msg

    def start_socket(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_TCP, socket.TCP_QUICKACK, 1)
        self.socket.connect((self.host, self.port))

        print('INFO: Socket connected to ' + self.host + ' on port ' + str(self.port))
        
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP
        print('INFO: UDP Socket created')

    def start_listen(self):
        self.should_stop = False
        t = threading.Thread(target=self.listen)
        t.daemon = True
        t.start()

        self.listen_thread = t

    def stop(self):
        self.should_stop = True
        self.listen_thread.join()
        self.socket.shutdown(socket.SHUT_RDWR)
        self.socket.close()

    def req_next_frame(self):
        # UDP message includes a newline at the end so that netcat can also be used
        # easily for debugging
        # 
        # send two packets for redundancy     
 
        timestamp = time.time()
        if (timestamp - self.last_frame_req_timestamp) > self.req_resend_timeout:     
            self.udp_socket.sendto(bytes("1\n", "utf-8"), (self.host, self.udp_port))
            self.last_frame_req_timestamp = timestamp

    @abc.abstractmethod
    def listen(self):
        return

    @abc.abstractmethod
    def get_mat_from_header(self, header):
        return


class VideoReceiverThread(FrameReceiverThread):
    def __init__(self, host):
        super().__init__(host, VIDEO_STREAM_PORT, VIDEO_UDP_PORT, VIDEO_STREAM_HEADER_FORMAT,
                         VIDEO_FRAME_STREAM_HEADER, VIDEO_REQUEST_TIMEOUT, sensor_name="VIDEO")

    def listen(self):
        count = 0
        start = time.time()
        
        while True:
            # self.req_next_frame()
            
            ret = self.get_data_from_socket()

            if ret is not None:
                # Mutex used so that the header and latest frame always match
                # when read by the other thread
                with self.lock:
                    self.latest_header, image_data = ret
                    self.latest_frame = np.frombuffer(image_data, dtype=np.uint8).reshape((self.latest_header.ImageHeight,
                                                                                        self.latest_header.ImageWidth,
                                                                                        self.latest_header.PixelStride))                                                          

                end = time.time()
                count += 1
                if (end-start) > FPS_PRINT_INTERVAL:
                    print(self.sensor_name, "receive FPS: ", count / (end-start))
                    count = 0
                    start = time.time()

            else:
                self.req_next_frame()


            if self.should_stop:
                return

    def get_mat_from_header(self, header):
        pv_to_world_transform = np.array(header[7:24]).reshape((4, 4)).T
        return pv_to_world_transform


    def get_data_from_socket(self, debug=False):
        # read image header
        reply = self.recvall(self.header_size, timeout=self.req_resend_timeout)


        if reply is None:
            # reply is None if self.recvall timed out
            # print(self.sensor_name, ": Header Timeout")
            return None

        data = struct.unpack(self.header_format, reply)
        header = self.header_data(*data)
        image_size_bytes = header.ImageHeight * header.RowStride

        # read the image
        image_data = self.recvall(header.BufLen, timeout=SOCKET_RESTART_TIMEOUT)

        
        if image_data is None:
            print(self.sensor_name, ": Image Timeout")

            global should_restart_sockets
            should_restart_sockets = True
            return None


        # max_uncompressed_size = 1952*1100 * 2

        # pass_1 = lz4.block.decompress(image_data, uncompressed_size=max_uncompressed_size)

        # qoi_image = lz4.block.decompress(pass_1, uncompressed_size=max_uncompressed_size)
    
        # bgr_decoded = qoi.decode(qoi_image)


        #temp
        bgr_decoded = image_data


        # print("BufLen_PV", header.BufLen)
        # print("bgr_decoded shape", bgr_decoded.shape)


        return header, bgr_decoded


class DepthReceiverThread(FrameReceiverThread):
    def __init__(self, host):
        super().__init__(host,
                         DEPTH_STREAM_PORT, DEPTH_UDP_PORT, RM_STREAM_HEADER_FORMAT, RM_FRAME_STREAM_HEADER,
                         DEPTH_REQUEST_TIMEOUT, sensor_name="DEPTH")

        self.latest_depth_frame = None
        self.latest_ab_frame = None


    def listen(self):
        count = 0
        start = time.time()


        while True:
            # self.req_next_frame()

            ret = self.get_data_from_socket()

            if ret is not None:
                # Mutex used so that the header and latest frame always match
                # when read by the other thread
                with self.lock:
                    self.latest_header, depth_data, ab_data = ret
                    self.latest_depth_frame = np.frombuffer(depth_data, dtype=np.uint16).reshape((self.latest_header.ImageHeight,
                                                                                    self.latest_header.ImageWidth))
                    self.latest_ab_frame = np.frombuffer(ab_data, dtype=np.uint16).reshape((self.latest_header.ImageHeight,
                                                                                    self.latest_header.ImageWidth))

                end = time.time()
                count += 1
                if (end-start) > FPS_PRINT_INTERVAL:
                    print(self.sensor_name, "receive FPS: ", count / (end-start))
                    count = 0
                    start = time.time()
                                                                                        
            else:
                self.req_next_frame()


            if self.should_stop:
                return

    def get_mat_from_header(self, header):
        rig_to_world_transform = np.array(header[5:22]).reshape((4, 4)).T
        return rig_to_world_transform


    def get_data_from_socket(self, debug=False):
        # THIS METHOD IS OVERRIDING THE FrameReceiverThread method

        # read image header
        reply = self.recvall(self.header_size, timeout=self.req_resend_timeout)
        
        if reply is None:
            # reply is None if self.recvall timed out
            # print(self.sensor_name, ": Header Timeout")

            return None

        data = struct.unpack(self.header_format, reply)
        header = self.header_data(*data)
        image_size_bytes = header.ImageHeight * header.RowStride

        # read the image
        image_data = self.recvall(header.BufLen, timeout=SOCKET_RESTART_TIMEOUT)
        
        
        if image_data is None:
            print(self.sensor_name, ": Image Timeout")

            global should_restart_sockets
            should_restart_sockets = True
            return None

        # print("BufLen", self.sensor_name, header.BufLen)

        # max_uncompressed_size = 512*512*4 * 2
        # pass_1 = lz4.block.decompress(image_data, uncompressed_size=max_uncompressed_size)
        # qoi_image = lz4.block.decompress(pass_1, uncompressed_size=max_uncompressed_size)
        # depth_combined_decoded = bytearray(qoi.decode(qoi_image))


        depth_combined_decoded = image_data


        depth_image = depth_combined_decoded[:image_size_bytes]
        ab_image = depth_combined_decoded[image_size_bytes:]



        return header, depth_image, ab_image



class VLC_ReceiverThread(FrameReceiverThread):
    def __init__(self, host, camera="LF"):

        if camera == "LF":
            super().__init__(host,
                         LEFT_FRONT_STREAM_PORT, LEFT_FRONT_UDP_PORT, RM_STREAM_HEADER_FORMAT,
                         RM_FRAME_STREAM_HEADER, VLC_REQUEST_TIMEOUT, sensor_name="VLC_LF")
                         
        elif camera == "RF":
            super().__init__(host,
                         RIGHT_FRONT_STREAM_PORT, RIGHT_FRONT_UDP_PORT, RM_STREAM_HEADER_FORMAT,
                         RM_FRAME_STREAM_HEADER, VLC_REQUEST_TIMEOUT, sensor_name="VLC_RF")

        else:
            print("Only LF and RF cameras implemented")


    def listen(self):
        count = 0
        start = time.time()


        while True:
            # self.req_next_frame()
            ret = self.get_data_from_socket()

            if ret is not None:
                # Mutex used so that the header and latest frame always match
                # when read by the other thread
                with self.lock:
                    self.latest_header, image_data = ret
                    self.latest_frame = np.frombuffer(image_data, dtype=np.uint8).reshape((self.latest_header.ImageHeight,
                                                                                            self.latest_header.ImageWidth))

                end = time.time()
                count += 1
                if (end-start) > FPS_PRINT_INTERVAL:
                    print(self.sensor_name, "receive FPS: ", count / (end-start))
                    count = 0
                    start = time.time()
            
            else:
                self.req_next_frame()


            if self.should_stop:
                return

    def get_mat_from_header(self, header):
        rig_to_world_transform = np.array(header[5:22]).reshape((4, 4)).T
        return rig_to_world_transform


    def get_data_from_socket(self, debug=False):
        # read image header
        reply = self.recvall(self.header_size, timeout=self.req_resend_timeout)

        if reply is None:
            # reply is None if self.recvall timed out
            # print(self.sensor_name, ": Header Timeout")

            return None

        data = struct.unpack(self.header_format, reply)
        header = self.header_data(*data)
        image_size_bytes = header.ImageHeight * header.RowStride

        # read the image
        image_data = self.recvall(header.BufLen, timeout=SOCKET_RESTART_TIMEOUT)

        if image_data is None:
            print(self.sensor_name, ": Image Timeout")

            global should_restart_sockets
            should_restart_sockets = True
            return None
        

        # max_uncompressed_size = 640*480 * 2
        # pass_1 = lz4.block.decompress(image_data, uncompressed_size=max_uncompressed_size)
        # qoi_image = lz4.block.decompress(pass_1, uncompressed_size=max_uncompressed_size)
        # vlc_decoded = qoi.decode(qoi_image)

        vlc_decoded = image_data


        # print("BufLen", self.sensor_name, header.BufLen)
        # print("vlc_decoded shape", vlc_decoded.shape, vlc_decoded.shape[1] * vlc_decoded.shape[2] * vlc_decoded.shape[0])

        return header, vlc_decoded






class HololensReceiver:

    def __init__(self, ip_address, cameras_to_stream):
        
        STREAM_VIDEO, STREAM_DEPTH, STREAM_FRONT_LEFT, STREAM_FRONT_RIGHT = cameras_to_stream
        
        self.receiver_list = []

        if STREAM_VIDEO:
            self.video_receiver = VideoReceiverThread(ip_address)
            self.receiver_list.append(self.video_receiver)


        if STREAM_DEPTH:
            self.depth_receiver = DepthReceiverThread(ip_address)
            self.receiver_list.append(self.depth_receiver)

        if STREAM_FRONT_LEFT:
            self.front_left_receiver = VLC_ReceiverThread(ip_address, camera="LF")
            self.receiver_list.append(self.front_left_receiver)

        if STREAM_FRONT_RIGHT:
            self.front_right_receiver = VLC_ReceiverThread(ip_address, camera="RF")
            self.receiver_list.append(self.front_right_receiver)

        # self.receiver_list = [self.video_receiver, self.depth_receiver, self.front_left_receiver, self.front_right_receiver]


        self.is_connected = False

        self.connect_to_hololens()


    def __bool__(self):
        return self.is_connected


    def _connect_to_hololens(self):
        print("Started Listening for HoloLens2...")

        for receiver in self.receiver_list:
            receiver.start_socket()

        for receiver in self.receiver_list:
            receiver.start_listen() 

    def connect_to_hololens(self):
        # This is only done in a thread so that ctrl-c can end the program otherwise
        # it will get stuck waiting to connect to the hololens
        t = threading.Thread(target=self._connect_to_hololens)
        t.daemon = True
        t.start()

        while True:
            t.join(0.5)
            if not t.is_alive():
                self.is_connected = True
                return
            else:
                self.is_connected = False
                return

    def _restart_sockets(self):
        print("Attempting to restart sockets...")
        for receiver in self.receiver_list:
            receiver.stop()            

        for receiver in self.receiver_list:
            receiver.start_socket()

        for receiver in self.receiver_list:
            receiver.start_listen()

        print("Reconnected to HoloLens2")


    def restart_sockets(self):
        # This is only done in a thread so that ctrl-c can end the program otherwise
        # it will get stuck waiting
        t = threading.Thread(target=self._restart_sockets)
        t.daemon = True
        t.start()

        while True:
            t.join(0.5)
            if not t.is_alive():
                break   

    
    def close_all_sockets(self):
        for receiver in self.receiver_list:
            receiver.stop() 