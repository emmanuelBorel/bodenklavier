import os
from primesense import openni2
import numpy as np
import cv2

OPENNI_PATH = os.environ["OPENNI2_REDIST"]

openni2.initialize(OPENNI_PATH)

dev = openni2.Device.open_any()
print("Caméra détectée :", dev.get_device_info().name)

depth_stream = dev.create_depth_stream()
depth_stream.set_video_mode(
    openni2.VideoMode(
        pixelFormat=openni2.PIXEL_FORMAT_DEPTH_1_MM,
        resolutionX=320,
        resolutionY=240,
        fps=30
    )
)

depth_stream.start()

try:
    while True:
        frame = depth_stream.read_frame()

        depth_data = frame.get_buffer_as_uint16()
        depth_array = np.frombuffer(depth_data, dtype=np.uint16).copy()
        depth_array = depth_array.reshape((240, 320))

        depth_vis = cv2.convertScaleAbs(depth_array, alpha=0.03)
        depth_vis = cv2.applyColorMap(depth_vis, cv2.COLORMAP_JET)

        cv2.imshow("Astra Depth", depth_vis)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

finally:
    depth_stream.stop()
    openni2.unload()
    cv2.destroyAllWindows()
