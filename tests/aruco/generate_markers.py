
import cv2
from pathlib import Path

output = Path("tests/aruco/markers")
output.mkdir(parents=True, exist_ok=True)

dictionary = cv2.aruco.getPredefinedDictionary(
    cv2.aruco.DICT_4X4_50
)

for marker_id in range(4):
    marker = cv2.aruco.generateImageMarker(
        dictionary, marker_id, 400
    )

    path = output / f"aruco_{marker_id}.png"
    cv2.imwrite(str(path), marker)
    print(f"Créé : {path}")
