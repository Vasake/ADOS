#include "JPEG.h"
#include "NxNDCT.h"
#include <math.h>

#include "JPEGBitStreamWriter.h"


#define DEBUG(x) do{ qDebug() << #x << " = " << x;}while(0)



// quantization tables from JPEG Standard, Annex K
uint8_t QuantLuminance[8*8] =
    { 16, 11, 10, 16, 24, 40, 51, 61,
      12, 12, 14, 19, 26, 58, 60, 55,
      14, 13, 16, 24, 40, 57, 69, 56,
      14, 17, 22, 29, 51, 87, 80, 62,
      18, 22, 37, 56, 68,109,103, 77,
      24, 35, 55, 64, 81,104,113, 92,
      49, 64, 78, 87,103,121,120,101,
      72, 92, 95, 98,112,100,103, 99 };
uint8_t QuantChrominance[8*8] =
    { 17, 18, 24, 47, 99, 99, 99, 99,
      18, 21, 26, 66, 99, 99, 99, 99,
      24, 26, 56, 99, 99, 99, 99, 99,
      47, 66, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99,
      99, 99, 99, 99, 99, 99, 99, 99 };

static char quantizationMatrix[64] =
{
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

struct imageProperties{
    int width;
    int height;
    int16_t* coeffs;
};


void DCTUandV(const char input[], int16_t output[], int N, double* DCTKernel)
{
    double* temp = new double[N*N];
    double* DCTCoefficients = new double[N*N];

    double sum;
    for (int i = 0; i <= N - 1; i++)
    {
        for (int j = 0; j <= N - 1; j++)
        {
            sum = 0;
            for (int k = 0; k <= N - 1; k++)
            {
                sum = sum + DCTKernel[i*N+k] * (input[k*N+j]);
            }
            temp[i*N + j] = sum;
        }
    }

    for (int i = 0; i <= N - 1; i++)
    {
        for (int j = 0; j <= N - 1; j++)
        {
            sum = 0;
            for (int k = 0; k <= N - 1; k++)
            {
                sum = sum + temp[i*N+k] * DCTKernel[j*N+k];
            }
            DCTCoefficients[i*N+j] = sum;
        }
    }

    for(int i = 0; i < N*N; i++)
    {
        output[i] = floor(DCTCoefficients[i]+0.5);
    }

    delete[] temp;
    delete[] DCTCoefficients;

    return;
}

uint8_t quantQuality(uint8_t quant, uint8_t quality) {
    // Convert to an internal JPEG quality factor, formula taken from libjpeg
    int16_t q = quality < 50 ? 5000 / quality : 200 - quality * 2;
    return clamp((quant * q + 50) / 100, 1, 255);
}

static void doZigZag(int16_t block[], uint8_t quantizationBlock[], int N, int DCTorQuantization)
{
    /* TO DO */
    static const uint8_t zigzagOrder[64] = {
             0,  1,  8, 16,  9,  2,  3, 10,
            17, 24, 32, 25, 18, 11,  4,  5,
            12, 19, 26, 33, 40, 48, 41, 34,
            27, 20, 13,  6,  7, 14, 21, 28,
            35, 42, 49, 56, 57, 50, 43, 36,
            29, 22, 15, 23, 30, 37, 44, 51,
            58, 59, 52, 45, 38, 31, 39, 46,
            53, 60, 61, 54, 47, 55, 62, 63
        };
    int16_t temp[N*N];

    if(DCTorQuantization==0)
    {
        for(int i=0;i<N*N;i++)
        {
            //temp[i]=(int16_t)floor((double)block[zigzagOrder[i]]/quantizationBlock[i]+0.5);
            temp[i]=quantizationBlock[zigzagOrder[i]];
        }
    }
    else
    {
        for(int i=0;i<N*N;i++)
        {
            //temp[i]=quantizationBlock[zigzagOrder[i]];
            temp[i]=block[zigzagOrder[i]];
            //temp[i]=(int16_t)floor((double)block[zigzagOrder[i]]/quantizationBlock[i]+0.5);
        }
    }

    memcpy(block,temp,N*N*sizeof(int16_t));
}

/* perform DCT */
imageProperties performDCT(char input[], int xSize, int ySize, int N, uint8_t quality, bool quantType)
{
	// TO DO
    //kvantizaciona matrica
    uint8_t Quanti[64];
    uint8_t* temp;
    if(quantType)
    {
        temp=QuantChrominance;
    }
    else
    {
        temp=QuantLuminance;
    }
    for(int i=0;i<64;i++)
    {
        Quanti[i]=quantQuality(temp[i],quality);
    }

    //Prosirinjavanje
    char* extened=nullptr;
    int x_newSize;
    int y_newSize;
    extendBorders(input,xSize,ySize,N,&extened,&x_newSize,&y_newSize);

    //DCT matrica
    double* DCTKernel=new double[N*N];
    GenerateDCTmatrix(DCTKernel,N);
    int16_t* coef=new int16_t[x_newSize*y_newSize];

    //prolaz
    for(int y=0;y<y_newSize;y+=N)
    {
        for(int x=0;x<x_newSize;x+=N)
        {
            char BlokUlaz[64];
            int16_t BlokIzlaz[64];

            for(int yy=0;yy<N;yy++)
            {
                for(int xx=0;xx<N;xx++)
                {
                    BlokUlaz[yy*N+xx]=extened[(y+yy)*x_newSize+(x+xx)];
                }
            }
            //DCT
            if(!quantType)
            {
                DCT(BlokUlaz,BlokIzlaz,N,DCTKernel);
            }
            else
            {
                DCTUandV(BlokUlaz,BlokIzlaz,N,DCTKernel);
            }

            //Kvantizacija
            for(int i=0;i<N*N;i++)
            {
                BlokIzlaz[i]=(int16_t)floor((double)BlokIzlaz[i]/Quanti[i]+0.5);
            }

            //ZigiZagi
            doZigZag(BlokIzlaz,Quanti,N,1);

            //Upis
            for(int yy=0;yy<N;yy++)
            {
                for(int xx=0;xx<N;xx++)
                {
                    coef[(y+yy)*x_newSize+(x+xx)]=BlokIzlaz[yy*N+xx];
                }
            }
        }
    }

    delete[] DCTKernel;
    delete[] extened;

    imageProperties izlazSlika;
    izlazSlika.width=x_newSize;
    izlazSlika.height=y_newSize;
    izlazSlika.coeffs=coef;
    return izlazSlika;
}

//JPEGBitStreamWriter streamer("example.jpg");
void performJPEGEncoding(uchar Y_buff[], char U_buff[], char V_buff[], int xSize, int ySize, int quality)
{
	DEBUG(quality);
    // TO DO

    int N =8;
    //priprema i fix za crashove kod slika nedeljivih sa 16
    //int X16=((xSize+15)/16)*16;
    //int Y16=((ySize+15)/16)*16;
    char* Y_produzen=nullptr;
    int x_new;
    int y_new;
    char* Y_signed=new char[xSize*ySize];
    for(int i=0;i<xSize*ySize;i++)
    {
        Y_signed[i]=(char)((int)Y_buff[i]-128);
    }
    extendBorders(Y_signed,xSize,ySize,16,&Y_produzen,&x_new,&y_new);
    //DCT
    imageProperties Y=performDCT(Y_produzen,x_new,y_new,N,quality,false);

    int uvX_size=Y.width/2;
    int uvY_size=Y.height/2;
    imageProperties U=performDCT(U_buff,uvX_size,uvY_size,N,quality,true);
    imageProperties V=performDCT(V_buff,uvX_size,uvY_size,N,quality,true);

    //qDebug() << "Y:" << Y.width << Y.height;
    //qDebug() << "U:" << U.width << U.height;
    //qDebug() << "V:" << V.width << V.height;
    //qDebug() << "uvxSize:" << uvX_size << "uvySize:" << uvY_size;
	
    //quant tabvles
    uint8_t quantY[N*N];
    uint8_t quantUV[N*N];
    for(int i=0;i<N*N;i++)
    {
        quantY[i]=quantQuality(QuantLuminance[i],quality);
        quantUV[i]=quantQuality(QuantChrominance[i],quality);
    }

    //ZigiZagi
    int16_t tempY[N*N];
    int16_t tempUV[N*N];
    for(int i=0;i<N*N;i++)
    {
        tempY[i]=quantY[i];
        tempUV[i]=quantUV[i];
    }
    doZigZag(tempY,quantY,N,0);
    doZigZag(tempUV,quantUV,N,0);
    for(int i=0;i<N*N;i++)
    {
        quantY[i]=tempY[i];
        quantUV[i]=tempUV[i];
    }

    //Header
    auto s = new JPEGBitStreamWriter("example.jpg");
    s->writeHeader();
    s->writeQuantizationTables(quantY,quantUV);
    s->writeImageInfo(xSize,ySize);
    s->writeHuffmanTables();

    //Loop
    for(int y=0;y<Y.height;y+=16)
    {
        for(int x=0;x<Y.width;x+=16)
        {
            //Y
            int Yoff[4][2]={{0,0},{0,8},{8,0},{8,8}};
            for(int k=0;k<4;k++)
            {
                int ky=Yoff[k][0];
                int kx=Yoff[k][1];
                int16_t Block[N*N];
                for(int yy=0;yy<N;yy++)
                {
                    for(int xx=0;xx<N;xx++)
                    {
                        Block[yy*N+xx]=Y.coeffs[(y+ky+yy)*Y.width+(x+kx+xx)];
                    }
                }
                s->writeBlockY(Block);
            }

            //U
            {
                int16_t Block[N*N];
                int uvX=x/2;
                int uvY=y/2;
                for(int yy=0;yy<N;yy++)
                {
                    for(int xx=0;xx<N;xx++)
                    {
                        Block[yy*N+xx]=U.coeffs[(uvY+yy)*U.width+(uvX+xx)];
                    }
                }
                s->writeBlockU(Block);
            }

            //V
            {
                int16_t Block[N*N];
                int uvX=x/2;
                int uvY=y/2;
                for(int yy=0;yy<N;yy++)
                {
                    for(int xx=0;xx<N;xx++)
                    {
                        Block[yy*N+xx]=V.coeffs[(uvY+yy)*V.width+(uvX+xx)];
                    }
                }
                s->writeBlockV(Block);
            }
        }
    }
    s->finishStream();

    delete[] Y.coeffs;
    delete[] U.coeffs;
    delete[] V.coeffs;
    delete[] Y_signed;
    delete[] Y_produzen;
    delete s;
}
