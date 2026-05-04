
#include "ImageProcessing.h"
#include "ColorSpaces.h"
#include "JPEG.h"

#include <cmath>

#include <QDebug>
#include <QString>
#include <QImage>

void imageProcessingFun(const QString& progName, QImage& outImgs, const QImage& inImgs, const QVector<double>& params)
{
    /* Create buffers for YUV image */
    uchar* Y_buff = new uchar[inImgs.width()*inImgs.height()];
    char* U_buff = new char[inImgs.width()*inImgs.height() / 4];
    char* V_buff = new char[inImgs.width()*inImgs.height() / 4];

    int X_SIZE = inImgs.width();
    int Y_SIZE = inImgs.height();

    /* Create empty output image */
    outImgs = QImage(inImgs.width(), inImgs.height(), inImgs.format());

    /* Convert input image to YUV420 image */
    RGBtoYUV420(inImgs.bits(), X_SIZE, Y_SIZE, Y_buff, U_buff, V_buff);

    //int N = round(params[0]);
    //if ((N % 2) == 0) N++;

    if(progName == QString("JPEG Encoder"))
    {
        /* Perform NxN DCT */
        performJPEGEncoding(Y_buff, U_buff, V_buff, X_SIZE, Y_SIZE, params[0]);
    }

    procesing_YUV420(Y_buff, U_buff, V_buff, inImgs.width(), inImgs.height(), 1, 1, 1);

    /* Convert YUV image back to RGB */
    YUV420toRGB(Y_buff, U_buff, V_buff, inImgs.width(), inImgs.height(), outImgs.bits());

    outImgs = QImage("example.jpg");

    /* Delete used memory buffers */
    delete[] Y_buff;
    delete[] U_buff;
    delete[] V_buff;
}

