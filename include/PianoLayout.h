#ifndef PIANO_LAYOUT_H
#define PIANO_LAYOUT_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct PianoKey
{
    std::string note;
    cv::Rect zone;
    bool isBlack;
};

class PianoLayout
{
public:
    PianoLayout(int width, int height);

    const std::vector<PianoKey>& getKeys() const;

    void draw(cv::Mat& image) const;

private:
    int layoutWidth;
    int layoutHeight;

    std::vector<PianoKey> keys;

    void generateWhiteKeys();
    void generateBlackKeys();
};

#endif