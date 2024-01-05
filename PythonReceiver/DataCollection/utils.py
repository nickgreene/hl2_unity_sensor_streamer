import os
from collections import namedtuple

ImageDataTuple = namedtuple('ImageData', ('sensor_name', 'header', 'data'))
RosDataTuple = namedtuple('RosData', ('listener_name', 'rosmsg'))


def create_unique_output_folder(output_path):
    if not os.path.isdir(output_path):
        os.makedirs(output_path)

    counter = 1
    subfolder_path = os.path.join(output_path,f"run_{counter}")
    while os.path.isdir(subfolder_path):
        subfolder_path = os.path.join(output_path,f"run_{counter}")
        counter += 1

    os.makedirs(subfolder_path)

    return subfolder_path
