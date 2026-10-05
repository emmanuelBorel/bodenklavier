#include "PianoLayout.h"

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

using namespace cv;
using namespace std;

PianoLayout::PianoLayout(int width, int height)
    : layoutWidth(width),
      layoutHeight(height)
{
    generateWhiteKeys();
    generateBlackKeys();
}

const vector<PianoKey>& PianoLayout::getKeys() const
{
    return keys;
}

void PianoLayout::generateWhiteKeys()
{
    const int whiteKeyCount = 14;

    const float keyWidth =
        static_cast<float>(layoutWidth) /
        static_cast<float>(whiteKeyCount);

    const vector<string> notes = {
        "C4", "D4", "E4", "F4", "G4", "A4", "B4",
        "C5", "D5", "E5", "F5", "G5", "A5", "B5"
    };

    for (int i = 0; i < whiteKeyCount; ++i)
    {
        const int x1 =
            static_cast<int>(i * keyWidth);

        const int x2 =
            static_cast<int>((i + 1) * keyWidth);

        Rect zone(
            x1,
            0,
            x2 - x1,
            layoutHeight
        );

        PianoKey key;
        key.note = notes[i];
        key.zone = zone;
        key.isBlack = false;

        keys.push_back(key);
    }
}

void PianoLayout::generateBlackKeys()
{
    const float whiteKeyWidth =
        static_cast<float>(layoutWidth) / 14.0f;

    const float blackKeyWidth =
        whiteKeyWidth * 0.60f;

    const int blackKeyHeight =
        static_cast<int>(
            layoutHeight * 0.60f
        );

    const vector<int> blackPositions = {
        1, 2, 3,
        5, 6,
        8, 9, 10,
        12, 13
    };

   const vector<string> blackNotes = {
    "BLACK_01", "BLACK_02", "BLACK_03",
    "BLACK_04", "BLACK_05",
    "BLACK_06", "BLACK_07", "BLACK_08",
    "BLACK_09", "BLACK_10"
};
    
    for (size_t i = 0; i < blackPositions.size(); ++i)
    {
        const int boundary =
            blackPositions[i];

        const float centerX =
            boundary * whiteKeyWidth;

        const int x =
            static_cast<int>(
                centerX -
                blackKeyWidth / 2.0f
            );

        Rect zone(
            x,
            0,
            static_cast<int>(blackKeyWidth),
            blackKeyHeight
        );

        PianoKey key;
        key.note = blackNotes[i];
        key.zone = zone;
        key.isBlack = true;

        keys.push_back(key);
    }
}

void PianoLayout::draw(Mat& image) const
{
    for (const auto& key : keys)
    {
        const Scalar color =
            key.isBlack
                ? Scalar(0, 0, 255)
                : Scalar(255, 255, 255);

        rectangle(
            image,
            key.zone,
            color,
            2
        );

        Point textPosition(
            key.zone.x + 3,
            key.zone.y +
                key.zone.height - 10
        );

        putText(
            image,
            key.note,
            textPosition,
            FONT_HERSHEY_SIMPLEX,
            0.4,
            color,
            1
        );
    }
}