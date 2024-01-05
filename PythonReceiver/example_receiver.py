import cv2
import numpy as np

from DataCollection.utils import *
from DataCollection.HololensReceiver import HololensReceiver, VLC_ReceiverThread, VideoReceiverThread, DepthReceiverThread

#########################################################
# Set the streams that you need to True 
# The depth stream contains two images, "AB" which is the Infrared image, and "Depth"
# which is the depth image (hard to see the raw image)
STREAM_VIDEO = True
STREAM_DEPTH = True
STREAM_FRONT_LEFT = True
STREAM_FRONT_RIGHT = True

# Set Hololens IP address
HOLOLENS_IP = "10.162.35.31"
#########################################################


if __name__ == '__main__':
    hl2_receiver = HololensReceiver(HOLOLENS_IP, (STREAM_VIDEO, STREAM_DEPTH, STREAM_FRONT_LEFT, STREAM_FRONT_RIGHT))

    if STREAM_VIDEO:
        cv2.namedWindow('Photo Video Camera Stream', cv2.WINDOW_NORMAL | cv2.WINDOW_GUI_NORMAL)

    if STREAM_DEPTH:
        cv2.namedWindow('Depth Camera Depth Stream', cv2.WINDOW_NORMAL | cv2.WINDOW_GUI_NORMAL)
        cv2.namedWindow('Depth Camera Ab Stream', cv2.WINDOW_NORMAL | cv2.WINDOW_GUI_NORMAL)

    if STREAM_FRONT_LEFT:
        cv2.namedWindow('Front Left Camera Stream', cv2.WINDOW_NORMAL | cv2.WINDOW_GUI_NORMAL)

    if STREAM_FRONT_RIGHT:
        cv2.namedWindow('Front Right Camera Stream', cv2.WINDOW_NORMAL | cv2.WINDOW_GUI_NORMAL)

    have_video_frame = have_depth_frame = have_front_left_frame = have_front_right_frame = False    
    should_restart_sockets = False

    if hl2_receiver:
        try:
            while True:
                if STREAM_VIDEO:
                    have_video_frame = np.any(hl2_receiver.video_receiver.latest_frame)
                    if have_video_frame:
                        cv2.imshow('Photo Video Camera Stream', hl2_receiver.video_receiver.latest_frame)

                if STREAM_DEPTH:
                    have_depth_frame = np.any(hl2_receiver.depth_receiver.latest_depth_frame) and np.any(hl2_receiver.depth_receiver.latest_ab_frame)
                    if have_depth_frame:
                        cv2.imshow('Depth Camera Depth Stream', hl2_receiver.depth_receiver.latest_depth_frame)
                        cv2.imshow('Depth Camera Ab Stream', hl2_receiver.depth_receiver.latest_ab_frame*2)

                if STREAM_FRONT_LEFT:
                    have_front_left_frame = np.any(hl2_receiver.front_left_receiver.latest_frame)
                    if have_front_left_frame:
                        rotated_image = cv2.rotate(hl2_receiver.front_left_receiver.latest_frame, cv2.ROTATE_90_CLOCKWISE)
                        cv2.imshow('Front Left Camera Stream', rotated_image)

                if STREAM_FRONT_RIGHT:
                    have_front_right_frame = np.any(hl2_receiver.front_right_receiver.latest_frame) 
                    if have_front_right_frame:
                        rotated_image = cv2.rotate(hl2_receiver.front_right_receiver.latest_frame, cv2.ROTATE_90_COUNTERCLOCKWISE)  
                        cv2.imshow('Front Right Camera Stream', rotated_image)

                key = cv2.waitKey(1) & 0xFF

                if key == ord('q'):
                    break
                elif key == ord('r'):
                    should_restart_sockets = True
                    hl2_receiver.restart_sockets()

           
        except KeyboardInterrupt:
            print("Caught KeyboardInterrupt")
            pass

        finally:
            if hl2_receiver:
                hl2_receiver.close_all_sockets()           

        print("closed sockets")
