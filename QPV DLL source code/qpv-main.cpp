﻿// qpv-main.cpp : Définit les fonctions exportées de la DLL.

#define GDIPVER 0x110
#include "pch.h"
#include "framework.h"
#include <wchar.h>
#include "omp.h"
#include "math.h"
#include "windows.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <Knownfolders.h>
#include <objbase.h>
#include <strsafe.h>
#include <shobjidl.h>
#include <oleidl.h>
#include <string>
#include <sstream>
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <array>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <algorithm>
#include <wincodec.h>
#include "Tchar.h"
#include "Tpcshrd.h"
#define GDIPVER 0x110
#include <gdiplus.h>
#include <gdiplusflat.h>
#include <locale.h>
#include <codecvt>
#include <corecrt_io.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include "Jpeg2PDF.h"
#include "Jpeg2PDF.cpp"
// #define cimg_plugin "include\gmic-master\gmic.cpp";
#define cimg_use_openmp 1
#include "include\CImg-3.3.1\CImg.h"
// #include "include\CImg-3.1.4\CImg.h"
// #include <bits/stdc++.h>
using namespace std;
using namespace cimg_library;
#define DLL_API extern "C" __declspec(dllexport)
#define DLL_CALLCONV __stdcall


void fnOutputDebug(std::string input) {
    std::stringstream ss;
    ss << "qpv: " << input;
    OutputDebugStringA(ss.str().data());
}

inline bool inRange(const float low, const float high, const float x) {
    return (low <= x && x <= high);
}

inline bool inRange(const int low, const int high, const int x) {
    return (low <= x && x <= high);
}

int inline weighTwoValues(float A, float B, float w) {
    if (w>=1)
       return A;
    else if (w<=0)
       return B;
    else
       return w * (A - B) + B;
       // return (A*w + B*(1.0f - w));
}

float inline weighTwoValues(float A, float B, float w, int r) {
    if (w>=1)
       return A;
    else if (w<=0)
       return B;
    else
       return w * (A - B) + B;
       // return (A*w + B*(1.0f - w));
}

static unsigned short gamma_to_linear[256];
static unsigned char linear_to_gamma[32769];
static float char_to_float[256];
static float char_to_grayRfloat[256];
static float char_to_grayGfloat[256];
static float char_to_grayBfloat[256];
static float int_to_float[65536];
static int LUTgamma[65536];
static int LUTgammaBright[65536];
static int LUTbright[65536];
static int LUTshadows[65536];
static int LUThighs[65536];
static int LUTcontra[65536];
static int int_to_char[65536];
static int char_to_int[256];
static int int_to_grayRi[65536];
static int int_to_grayGi[65536];
static int int_to_grayBi[65536];
static int linear_to_gammaInt16[65536];
static int gamma_to_linearInt16[65536];

IWICImagingFactory *m_pIWICFactory;

DLL_API int DLL_CALLCONV initWICnow(UINT modus, int threadIDu) {
    HRESULT hr = S_OK;
    // to do - fix this; make it work on Windows 7 

    if (SUCCEEDED(hr))
    {
       hr = CoCreateInstance(CLSID_WICImagingFactory,
                    NULL, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&m_pIWICFactory));
    }

    // source https://www.teamten.com/lawrence/graphics/gamma/
    static const float GAMMA = 2.0;
    int result;
    for (int i = 0; i < 32769; i++) {
        result = (int)(pow(i/32768.0, 1/GAMMA)*255.0 + 0.5);
        linear_to_gamma[i] = (unsigned char)result;
    }

    for (int i = 0; i < 256; i++) {
        char_to_float[i] = i/255.0f;
        result = (int)(pow(char_to_float[i], GAMMA)*32768.0f + 0.5f);
        gamma_to_linear[i] = (unsigned short)result;
        char_to_grayRfloat[i] = i*0.299701f;
        char_to_grayGfloat[i] = i*0.587130f;
        char_to_grayBfloat[i] = i*0.114180f;
        char_to_int[i] = char_to_float[i] * 65535.0f;
    }

    for (int i = 0; i < 65536; i++) {
        int_to_float[i] = (float)i/65535.0f;
        int_to_char[i] = int_to_float[i] * 255.0f;
        int_to_grayRi[i] = i*0.299701f;
        int_to_grayGi[i] = i*0.587130f;
        int_to_grayBi[i] = i*0.114180f;

        result = (int)(pow(int_to_float[i], 1.0f/GAMMA)*65535.0f + 0.5f);
        linear_to_gammaInt16[i] = result;
        result = (int)(pow(int_to_float[i], GAMMA)*65535.0f + 0.5f);
        gamma_to_linearInt16[i] = result;
    }

    // std::stringstream ss;
    // ss << "qpv: threadu - " << threadIDu << " HRESULT " << hr;
    // OutputDebugStringA(ss.str().data());
    if (SUCCEEDED(hr))
       return 1;
    else 
       return 0;
}

int inline getGrayscale(int r, int g, int b) {
    return clamp(char_to_grayRfloat[r] + char_to_grayGfloat[g] + char_to_grayBfloat[b], 0.0f, 255.0f);
}


int inline brightMaths(int i, float fintensity) {
    return clamp(i + round( (float)i*fintensity ), 0.0f, 255.0f);
}

int inline contraMaths(int i, float fintensity, float deviation) {
    return clamp(floor( (float)fintensity * (i - 128.0f) ) + deviation, 0.0f, 255.0f);
}

int inline gammaMaths(int i, double gamma) {
    return round(255.0f * pow(char_to_float[i], gamma));
}

int inline getInt16grayscale(int r, int g, int b) {
    return clamp((int)(int_to_grayRi[r] + int_to_grayGi[g] + int_to_grayBi[b]), 0, 65535);
}
int inline brightMathsInt16(int i, float fintensity) {
    return clamp(i + (float)i*fintensity, 0.0f, 65535.0f);
}

int inline contraMathsInt16(int i, float fintensity, float deviation) {
    return clamp(floor( (float)fintensity * (i - 32768.0f) ) + deviation, 0.0f, 65535.0f);
}

int inline gammaMathsInt16(int i, double gamma) {
    return round(65535.0f * pow(int_to_float[i], gamma));
}


#include "qpv-main.h"

inline INT64 CalcPixOffset(int x, int y, INT64 Stride, INT64 bitsPerPixel) {
    return y * Stride + x * (bitsPerPixel / 8);
}

bool isInsideRectOval(float ox, float oy, int modus) {
    // Translate the coordinates
    // if (excludeSelectScale!=0)
    // {
    //    if (inRange(imgSelX1 + imgSelExclX, imgSelX1 + imgSelW - imgSelExclX*2, ox) || inRange(imgSelY1 + imgSelExclY, imgSelY1 + imgSelH - imgSelExclY*2, oy))
    //       return 0;
    // }

    float tw = (modus==2) ? imgSelExclW : hImgSelW;
    float th = (modus==2) ? imgSelExclH : hImgSelH;
    float x = (modus==2) ? ox - tw - imgSelExclX : ox - tw;
    float y = (modus==2) ? oy - th - imgSelExclY : oy - th;
    x *= imgSelXscale;
    y *= imgSelYscale;

    // Apply rotation to the coordinates
    float rotatedX, rotatedY;
    if (flippedSelection==1)
    {
       rotatedX = x * cosVPselRotation + y * sinVPselRotation;
       rotatedY = x * sinVPselRotation - y * cosVPselRotation;
    } else
    {
       rotatedX = x * cosVPselRotation - y * sinVPselRotation;
       rotatedY = x * sinVPselRotation + y * cosVPselRotation;
    }

    bool f;
    float result;
    if (EllipseSelectMode==1)
    {
       result = (rotatedX * rotatedX) / (tw * tw) + (rotatedY * rotatedY) / (th * th);
       f = (result <= 1.0f);
    } else
    {
       f = ((fabs(rotatedX) < tw) && (fabs(rotatedY) < th));
    }

    if (f && modus==1 && excludeSelectScale!=0)
    {
       bool nf = isInsideRectOval(ox, oy, 2);
       if (f && nf)
          return 0;
       else 
          return 1;
    }

    return f;
}


/*
pBitmap and pBitmapMask must be the same width and height
and in 32-ARGB format: PXF32ARGB - 0x26200A.

The alpha channel will be applied directly on the pBitmap provided.

For best results, pBitmapMask should be grayscale.
*/

DLL_API int DLL_CALLCONV SetBitmapAsAlphaChannel(unsigned char *imageData, unsigned char *maskData, int w, int h, int Stride, int bpp, int invert, int replaceAlpha, int whichChannel) {
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = 0; x < w; x++)
    {
        unsigned char alpha, alpha2;
        for (int y = 0; y < h; y++)
        {
            INT64 px = CalcPixOffset(x, y, Stride, bpp);
            if (whichChannel==2)
               alpha = maskData[px + 1]; // green
            else if (whichChannel==3)
               alpha = maskData[px];     // blue
            else if (whichChannel==4)
               alpha = maskData[px + 3]; // alpha
            else
               alpha = maskData[px + 2]; // red

            if (replaceAlpha!=1)
            {
               if (invert == 1)
                  alpha = 255 - alpha;
               int a = imageData[px + 3];
               alpha2 = min(alpha, a);    // handles bitmaps that already have alpha
            }
            else {
               alpha2 = (invert == 1) ? 255 - alpha : alpha;
            }

            imageData[px + 3] = alpha2;
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV SetColorAlphaChannel(int *imageData, int w, int h, int newColor, int invert) {
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            INT64 px = x + y * w;
            unsigned char alpha1 = (imageData[px] >> 16) & 0xFF; // red
            alpha1 = (invert==1) ? 255 - alpha1 : alpha1;
            unsigned char alpha2 = (newColor >> 24) & 0xFF; // alpha
            // imageData[px] = newColor;
            imageData[px] = (min(alpha1,alpha2) << 24) | (newColor & 0x00ffffff);
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV AlterBitmapAlphaChannel(unsigned char *imageData, int w, int h, int Stride, int bpp, int level, int replaceAlpha) {
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            INT64 px = CalcPixOffset(x, y, Stride, bpp);
            if (imageData[px + 3]==0)
               continue;

            if (replaceAlpha==1)
               imageData[px + 3] = level;
            else
               imageData[px + 3] = clamp(imageData[px + 3] + level, 0, 255);
        }
    }

    return 1;
}

void dumbFillPixel(int x, int y, int r, int g, int b, unsigned char *imageData, int Stride, int bpp, int w, int h) {
    if (!inRange(0, w - 1, x))
       return;
    if (!inRange(0, h - 1, y))
       return;

    INT64 px = CalcPixOffset(x, y, Stride, bpp);
    imageData[px] = b;
    imageData[px + 1] = g;
    imageData[px + 2] = r;
    if (bpp==32)
       imageData[px + 3] = 255;
}

void plotLineSetPixel(int width, int height, int nx, int ny) {
    if (ny<0 || ny>height)
       return;

    // polygonMapMax[ny] = max(polygonMapMax[ny], nx);
    polygonMapMin[ny] = min(polygonMapMin[ny], nx);
    // polygonMapTempMin[ny] = min(polygonMapTempMin[ny], nx);
    // fnOutputDebug("maxu=" + std::to_string(polygonMapMax[ny]) + " | minu=" + std::to_string(polygonMapMin[ny]));

    if (nx>=polyW || ny>=polyH || nx<polyX || ny<polyY || nx>=width || ny>=height || nx<0 || ny<0)
       return;

    // UINT64 index = (UINT64)(ny - polyY) * polyW + (nx - polyX);
    polygonMaskMap[(UINT64)(ny - polyY) * polyW + (nx - polyX)] = 1;

    // try {
    //     int value = polygonMaskMap.at(index); // Check if defined at index
    // } catch (const std::out_of_range& e) {
    //     fnOutputDebug(std::to_string(index) + "= error plotLineSetPixel: " + std::to_string(nx) + " // " + std::to_string(ny));
    // }

    // polygonMaskMap[(INT64)ny * width + nx] = 1;
    // dumbFillPixel(nx, ny, 255, 255, 0, imageData, Stride, bpp, width, height);
}

void plotLineLow(int w, int h, int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int yi = 1;
    if (dy < 0)
    {
        yi = -1;
        dy = -dy;
    }

    int D = (2 * dy) - dx;
    int y = y0;
    for (int x=x0; x<=x1; x++)
    {
        plotLineSetPixel(w, h, x, y);
        if (D > 0)
        {
            y = y + yi;
            D = D + (2 * (dy - dx));
        } else
        {
            D = D + 2*dy;
        }
    }
}

void plotLineHigh(int w, int h, int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int xi = 1;
    if (dx < 0)
    {
        xi = -1;
        dx = -dx;
    }

    int D = (2 * dx) - dy;
    int x = x0;
    for (int y=y0; y<=y1; y++)
    {
        plotLineSetPixel(w, h, x, y);
        if (D > 0)
        {
            x = x + xi;
            D = D + (2 * (dx - dy));
        } else
        {
            D = D + 2*dx;
        }
    }
}

void bresenham_line_algo(int w, int h, int x0, int y0, int x1, int y1) {
// code adapted from https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

    if (abs(y1 - y0) < abs(x1 - x0))
    {
        if (x0 > x1)
           plotLineLow(w, h, x1, y1, x0, y0);
        else
           plotLineLow(w, h, x0, y0, x1, y1);
    } else
    {
        if (y0 > y1)
           plotLineHigh(w, h, x1, y1, x0, y0);
        else
           plotLineHigh(w, h, x0, y0, x1, y1);
    }
}

bool isPointInPolygon(INT64 pX, INT64 pY, float* PointsList, int PointsCount) {
// based on https://stackoverflow.com/questions/217578/how-can-i-determine-whether-a-2d-point-is-within-a-polygon
// https://wrf.ecse.rpi.edu/Research/Short_Notes/pnpoly.html
// thank you VERY MUCH , Michael Katz <3

    bool inside = false;
    for ( int i = 0; i < PointsCount*2; i+=2)
    {
        int j = i - 2;
        if (j<0)
           j = PointsCount*2 - 2;

        int xi = PointsList[i];
        int yi = PointsList[i + 1];
        int xj = PointsList[j];
        int yj = PointsList[j + 1];
        // fnOutputDebug("xi/yi=" + std::to_string(xi) + "/" + std::to_string(yi));
        // fnOutputDebug("xj/yj=" + std::to_string(xj) + "/" + std::to_string(yj));
        if ( ( yi > pY ) != ( yj > pY ) && pX < ( xj - xi ) * ( pY - yi ) / ( yj - yi ) + xi )
           inside = !inside;
    }
    return inside;
}

void FillMaskPolygon(int w, int h, float* PointsList, int PointsCount, int ppx1, int ppy1, int ppx2, int ppy2) {

    fnOutputDebug("FillMaskPolygon() invoked; PointsCount=" + std::to_string(PointsCount));
    INT64 s = (INT64)polyW * polyH + 2;
    if (s!=polygonMaskMap.size())
    {
       try {
          polygonMaskMap.resize(s);
       } catch(const std::bad_alloc& e) {
          EllipseSelectMode = 0;
          fnOutputDebug("polygonMaskMap failed. bad_alloc =" + std::to_string(s));
          return;
       } catch(const std::length_error& e) {
          EllipseSelectMode = 0;
          fnOutputDebug("polygonMaskMap failed. length_error =" + std::to_string(s));
          return;
       }

       fnOutputDebug("polygonMaskMap RESIZED=" + std::to_string(s) + "||" + std::to_string(polygonMaskMap.size()));
    } else
    {
       fnOutputDebug("polygonMaskMap size=" + std::to_string(s) + "||" + std::to_string(polygonMaskMap.size()));
    }

    fill(polygonMaskMap.begin(), polygonMaskMap.end(), 0);
    fnOutputDebug("polygonMaskMap refilled to zero ; size = " + std::to_string(s) + "|" + std::to_string(polyW) + " x " + std::to_string(polyH) + "|" + std::to_string(polyX) + " x " + std::to_string(polyY));

    int boundMaxX = 0;
    int boundMaxY = 0;
    int boundMinX = INT_MAX;
    int boundMinY = INT_MAX;
    for ( int i = 0; i < PointsCount*2; i+=2)
    {
        PointsList[i] = clamp(round(PointsList[i]), 0.0f, (float)w);
        PointsList[i + 1] = clamp(round(PointsList[i + 1]) + polyOffYa - polyOffYb, 0.0f, (float)h);
        boundMaxX = max(PointsList[i], boundMaxX);
        boundMaxY = max(PointsList[i + 1], boundMaxY);
        boundMinX = min(PointsList[i], boundMinX);
        boundMinY = min(PointsList[i + 1], boundMinY);
    }

    std::vector<std::unordered_set<int>>  polygonMapEdges;
    int hmax = max(boundMaxY, h) + 1;
    fnOutputDebug(std::to_string(hmax) + "=hmax; bound rect={" + std::to_string(boundMinX) + "," + std::to_string(boundMinY) + "," + std::to_string(boundMaxX) + "," + std::to_string(boundMaxY) + "}");
    polygonMapMin.resize(hmax);
    fnOutputDebug("polygonMapMin reserved");
    polygonMapEdges.reserve(hmax);
    for (int i=0; i<hmax; i++)
    {
        polygonMapEdges.emplace_back();
    }

    fnOutputDebug("polygonMapEdges reserved");
    fnOutputDebug("tracing polygonal path with bresenham algo");
    int i = 2;
    int xa = PointsList[0];
    int ya = PointsList[1];
    for (int pts = 0; pts < PointsCount;)
    {
        polygonMapMin.assign(h, INT_MAX);
        int xb = PointsList[i];
        i++;
        int yb = PointsList[i];
        i++;
        if (pts==PointsCount - 1)
        {
           xb = PointsList[0];
           yb = PointsList[1];
        }

        pts++;
        if (max(ya, yb)<ppy1 || min(ya, yb)>ppy2)
        // if (max(ya, yb)<ppy1 || min(ya, yb)>ppy2 || min(xa, xb)>ppx2 && polygonMapMin[ya]!=INT_MAX && polygonMapMin[yb]!=INT_MAX)
        // if ((max(xa, xb)<ppx1 || max(ya, yb)<ppy1) || (min(xa, xb)>ppx2 || min(ya, yb)>ppy2))
        {
           // fnOutputDebug(" poly segment skipped=" + std::to_string(pts));
           xa = xb;
           ya = yb;
           continue;
        }

        // fnOutputDebug("seg[ " + std::to_string(i) + "@" + std::to_string(pts) + " ]=( " + std::to_string(xa) + " | " + std::to_string(ya) + ", " + std::to_string(xb) + " | " + std::to_string(yb) + ");");
        bresenham_line_algo(w, h, xa, ya, xb, yb);
        int maxu = (max(ya, yb) >= ppy2) ? ppy2 - 1 : max(ya, yb);
        int minu = (min(ya, yb) <= ppy1) ? ppy1 : min(ya, yb);
        for (int yy = minu; yy <= maxu; yy++)
        {
            if (polygonMapMin[yy]!=INT_MAX)
               polygonMapEdges[yy].emplace( polygonMapMin[yy] );
        }

        xa = xb;
        ya = yb;
        if (pts>=PointsCount || i>PointsCount*2)
           break;
    }

    fnOutputDebug("fill mask image using the list of x-pairs identified and stored in polygonMapEdges");
    int countPIPcalls = 0;
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int y = 0; y < h; ++y)
    {
        if (polygonMapEdges[y].empty())
        {
           // fnOutputDebug("empty Y=" + std::to_string(y));
           continue;
        }

        if (y<=ppy1 || y>=ppy2)
        {
           // fnOutputDebug("out of ppy range; Y=" + std::to_string(y));
           continue;
        }

        std::vector<int> listu;
        listu.assign(polygonMapEdges[y].begin(), polygonMapEdges[y].end());
        if (listu.empty())
        {
           // fnOutputDebug("empty list at Y=" + std::to_string(y));
           continue;
        }

        // std::stringstream ss;
        // fnOutputDebug(std::to_string(h) + "=h ; " + std::to_string(listu.size()) + " list size Y=" + std::to_string(y));
        if (listu.size()==1)
        {
           // fnOutputDebug(" one element list at Y=" + std::to_string(y));
           continue;
        }

        sort(listu.begin(), listu.end()); 
        for (INT64 i = 0; i < listu.size() - 1; i++)
        {
             INT64 xa = listu[i];
             INT64 xb = listu[i + 1];
             if (xb==xa)
             {
                // fnOutputDebug("skipped identical xa/xb, Y=" + std::to_string(y));
                continue;
             }

             if (max(xa,xb)<ppx1 || min(xa,xb)>=ppx2)
             {
                // fnOutputDebug("xa/xb out of ppx range; skipped Y=" + std::to_string(y));
                continue;
             }

             // we could always say these are to be filled [the first pair with (i>0) and the last pair (i=listu.size - 1)]
             // but there are corner cases which screw it up
             if (listu.size()>2)
             {
                 // countPIPcalls++;
                 if (!isPointInPolygon((xa + xb)/2, y, PointsList, PointsCount))
                    continue;

                 // dumbFillPixel(midX, y, 200, 200, 255, imageData, Stride, bpp, w, h);
             }

             if (xb<xa)
                swap(xa,xb);

             // fnOutputDebug(std::to_string(midX) + "=midX == yaaaaay");
             for (INT64 x = xa; x <= xb; x++)
             {
                  if (x<=ppx1 || x>=ppx2)
                  {
                     // if (x>=ppx2)
                     //    fnOutputDebug("x out of ppx range; x=" + std::to_string(x));
                     continue;
                  }

                  // if (x==xa || x==xb)
                  //    dumbFillPixel(x, y, 255, 0, 255, imageData, Stride, bpp, w, h);
                  // else
                  //    dumbFillPixel(x, y, 128, 0, 10, imageData, Stride, bpp, w, h);

                  polygonMaskMap[(INT64)(y - polyY) * polyW + x - polyX] = 1;
             }
             // if (listu.size()>2)
             //    dumbFillPixel(midX, y, 0, 255, 255, imageData, Stride, bpp, w, h);
        }
        // OutputDebugStringA(ss.str().data());
    }

    fnOutputDebug("fill mask image - done; calls to isPointInPolygon() executed: " + std::to_string(countPIPcalls));
    polygonMapEdges.clear();
    polygonMapEdges.shrink_to_fit();
    fnOutputDebug("polygonMapEdges discarded");
    // return 1;
}

DLL_API int DLL_CALLCONV discardFilledPolygonCache(int m) {
    polygonMapMin.clear();
    polygonMapMin.shrink_to_fit();
    polygonMaskMap.clear();
    polygonMaskMap.shrink_to_fit();
    return 1;
}

DLL_API int DLL_CALLCONV testFilledPolygonCache(int m) {
    int r = 1;
    if (polygonMaskMap.size()<2000 || polygonMapMin.size()<100)
       r = 0;
    return r;
}

bool clipMaskFilter(int x, int y, unsigned char *maskBitmap, int mStride) {
    if (invertSelection==1)
    {
       if (inRange(imgSelX1, imgSelX2, x) && inRange(imgSelY1, imgSelY2, y))
       {
          if (maskBitmap!=NULL)
          {
             INT64 mo = CalcPixOffset(x - imgSelX1, y - imgSelY1, mStride, 24);
             if (maskBitmap[mo]>128)
                return 1;
          } else if (EllipseSelectMode==2)
          {
             bool r = 0;
             if (inRange(0, polyH - 1, y - imgSelY1 - polyY) && inRange(0, polyW - 1, x - imgSelX1 - polyX))
                r = (polygonMaskMap[(INT64)(y - imgSelY1 - polyY) * polyW + x - imgSelX1 - polyX]) ? 1 : 0;
             return r;
          } else if (EllipseSelectMode==1 || EllipseSelectMode==0 && (vpSelRotation!=0 || excludeSelectScale!=0))
          {
             return isInsideRectOval(x - imgSelX1, y - imgSelY1, 1);
          } else 
          {
             return 1;
          }
       }
    } else
    {
       if (!inRange(imgSelX1, imgSelX2, x) || !inRange(imgSelY1 - polyOffYa, imgSelY2, y))
          return 1;

       if (maskBitmap!=NULL)
       {
          INT64 mo = CalcPixOffset(x - imgSelX1, y - imgSelY1, mStride, 24);
          if (maskBitmap[mo]<128)
             return 1;
       } else if (EllipseSelectMode==2)
       {
          bool r = 1;
          if (inRange(0, polyH - 1, y - imgSelY1 - polyY + polyOffYa) && inRange(0, polyW - 1, x - imgSelX1 - polyX))
             r = (polygonMaskMap[(INT64)(y - imgSelY1 - polyY + polyOffYa) * polyW + x - imgSelX1 - polyX]) ? 0 : 1;

           // fnOutputDebug("clipMaskFilter y=" + std::to_string(y - imgSelY1 - polyY + polyOffYa));
          return r;
       } else if (EllipseSelectMode==1 || EllipseSelectMode==0 && (vpSelRotation!=0 || excludeSelectScale!=0))
       {
          return !isInsideRectOval(x - imgSelX1, y - imgSelY1, 1);
       }
    }
    return 0;
}

double inverseGamma(double X) {
  // Inverse sRGB gamma correction
  if (X>0.0404482362771076)
     X = pow((X + 0.055)/1.055, 2.4);
  else
     X = X / 12.92;

  return X;
}

double toLABf(double Y) {
  if (Y >= 0.00885645167903563082e-3)
     Y = pow(Y, 0.333333333333333);  // 1/3
  else
     Y = (841.0/108.0) * Y + (4.0/29.0);

  return Y;
}

double toLABfx(double Y) {
  if (Y >= 8.88564517)
     Y = pow(Y, 0.3333333);  // 1/3
  else
     Y = 7.7870370 * Y + 0.1379310; // (841.0/108.0) * Y + ( 4.0 / 29.0 );

  return Y;
}

double deg2rad(double degree) {
    // convert degree to radian
    const double p = M_PI / 180.0;
    return (degree * p);
}

double rad2deg(double radian) {
    // convert radian to degree
    const double p = 180.0 / M_PI;
    return (radian * p);
}

int fastRGBtoGray(int n) {
   return (((n&0xf0f0f0)*0x20a05)>>20)&255;
   // return (n&0xf0f0f0)*133637>>20&255;
}

int RGBtoGray(int sR, int sG, int sB, int alternateMode) {
  // https://getreuer.info/posts/colorspace/index.html
  // http://www.easyrgb.com/en/math.php
  // sR, sG and sB (Standard RGB) input range [0, 255]
  // X, Y and Z output refer to a D65/2° standard illuminant.
  // return value is L* - Luminance from L*ab, based on D65 luminant

  if (alternateMode==1)
     return round(char_to_grayRfloat[sR] + char_to_grayGfloat[sG] + char_to_grayBfloat[sB]); // weighted grayscale conversion

  // convert RGB to XYZ color space
  double var_R = char_to_float[sR];
  double var_G = char_to_float[sG];
  double var_B = char_to_float[sB];

  // Inverse sRGB gamma correction
  var_R = inverseGamma(var_R);
  var_G = inverseGamma(var_G);
  var_B = inverseGamma(var_B);

  double Y = var_R * 0.2125862 + var_G * 0.7151704 + var_B * 0.0722005;
  // if (alternateMode==2)
  //    return round(Y); // return derived luminosity in XYZ color space

  Y = toLABfx(Y);
  double L = 116.0*Y - 16.0;
  // if (L>499)
  //     L = 499;
  // else if (L<0)
  //     L = 1;
  // https://zschuessler.github.io/DeltaE/demos/
  return round(L/2); // return derived luminosity in pseudo-LAB color space
}

double CieLab2Hue(double var_a, double var_b ) {
// Function returns CIE-H° value
   double var_bias = 0;
   if ( var_a >= 0 && var_b == 0 ) return 0;
   if ( var_a <  0 && var_b == 0 ) return 180;
   if ( var_a == 0 && var_b >  0 ) return 90;
   if ( var_a == 0 && var_b <  0 ) return 270;
   if ( var_a >  0 && var_b >  0 ) var_bias = 0;
   if ( var_a <  0               ) var_bias = 180;
   if ( var_a >  0 && var_b <  0 ) var_bias = 360;
   return ( rad2deg( atan( var_b / var_a ) ) + var_bias );
}

float testCIEdeltaE2000(double Lab1_L, double Lab1_a, double Lab1_b, double Lab2_L, double Lab2_a, double Lab2_b, float k_L, float k_C, float k_H) {
// Cl_1,  Ca_1,  Cb_1   - Color #1 CIE-L*ab values
// Cl_2,  Ca_2,  Cb_2   - Color #2 CIE-L*ab values
// WHT_L, WHT_C, WHT_H  - Weight factors: luminance, chroma and hue
// https://github.com/tajmone/name-that-color/blob/master/ntc.color-funcs.pbi

    double LBar, deltaLPrime, aPrime1, aPrime2;
    double C1, C2, CPrime1, CPrime2, CBar, CBarPrime, deltaCPrime;
    double hPrime1, hPrime2, HBarPrime, deltahPrime;
    double SsubL, SsubC, SsubH, RsubC, RsubT;
    double g, Tvar, deltaRO, deltaE00;

    LBar      = (Lab1_L + Lab2_L) / 2;
    C1        = sqrt(pow(Lab1_a, 2) + pow(Lab1_b, 2));
    C2        = sqrt(pow(Lab2_a, 2) + pow(Lab2_b, 2));
    CBar      = (C1 + C2) / 2;
    g = (1 - sqrt(pow(CBar, 7) / (pow(CBar, 7) + pow(25, 7)))) / 2;
    aPrime1   = Lab1_a * (1 + g);
    aPrime2   = Lab2_a * (1 + g);
    CPrime1   = sqrt(pow(aPrime1, 2) + pow(Lab1_b, 2));
    CPrime2   = sqrt(pow(aPrime2, 2) + pow(Lab2_b, 2));
    CBarPrime = (CPrime1 + CPrime2) / 2;

    hPrime1 = rad2deg(atan2(aPrime1, Lab1_b));
    if (hPrime1<0)
       hPrime1 += 360;

    hPrime2 = rad2deg(atan2(aPrime2, Lab2_b));
    if (hPrime2 < 0)
       hPrime2 += 360;

    if (abs(hPrime1 - hPrime2) > 180)
       HBarPrime = (hPrime1 + hPrime2 + 360) / 2;
    else
       HBarPrime = (hPrime1 + hPrime2) / 2;

    Tvar = 1 - 0.17 * cos(deg2rad(HBarPrime - 30)) + 0.24 * cos(deg2rad(2 * HBarPrime)) + 0.32 * cos(deg2rad(3 * HBarPrime + 6)) - 0.2 * cos(deg2rad(4 * HBarPrime - 63));

    deltahPrime = hPrime2 - hPrime1;
    if (abs(deltahPrime) > 180)
    {
       if (hPrime2 <= hPrime1) 
          deltahPrime += 360;
       else 
          deltahPrime -= 360;
    }

    deltaLPrime = Lab2_L - Lab1_L;
    deltaCPrime = CPrime2 - CPrime1;
    deltahPrime = 2 * sqrt(CPrime1 * CPrime2) * sin(deg2rad(deltahPrime) / 2);
    SsubL = 1 + ((0.015 * pow(LBar - 50, 2)) / sqrt(20 + pow(LBar - 50, 2)));
    SsubC = 1 + 0.045 * CBarPrime;
    SsubH = 1 + 0.015 * CBarPrime * Tvar;

    // compute R sub T (RT)
    deltaRO = 30 * exp(-(pow((HBarPrime - 275) / 25, 2)));
    RsubC = 2 * sqrt(pow(CBarPrime, 7) / (pow(CBarPrime, 7) + pow(25, 7)));
    RsubT = -RsubC * sin(2 * deg2rad(deltaRO));

    deltaE00 = sqrt(pow(deltaLPrime / (SsubL * k_L), 2) + pow(deltaCPrime / (SsubC * k_C), 2) + pow(deltahPrime / (SsubH * k_H), 2) + RsubT * (deltaCPrime / (SsubC * k_C)) * (deltahPrime / (SsubH * k_H)));


    // std::stringstream ss;
    // ss << "qpv: deltaE00=" << deltaE00;
    // ss << " Lab1_L=" << Lab1_L;
    // ss << " Lab1_a=" << Lab1_a;
    // ss << " Lab1_b=" << Lab1_b;
    // ss << " Lab2_L=" << Lab2_L;
    // ss << " Lab2_a=" << Lab2_a;
    // ss << " Lab2_b=" << Lab2_b;
    // ss << " k_L=" << k_L;
    // ss << " k_C=" << k_C;
    // ss << " k_H=" << k_H;
    // ss << " LBar=" << LBar;
    // ss << " deltaLPrime=" << deltaLPrime;
    // ss << " aPrime1=" << aPrime1;
    // ss << " aPrime2=" << aPrime2;
    // ss << " C1=" << C1;
    // ss << " C2=" << C2;
    // ss << " CPrime1=" << CPrime1;
    // ss << " CPrime2=" << CPrime2;
    // ss << " CBar=" << CBar;
    // ss << " CBarPrime=" << CBarPrime;
    // ss << " deltaCPrime=" << deltaCPrime;
    // ss << " hPrime1=" << hPrime1;
    // ss << " hPrime2=" << hPrime2;
    // ss << " HBarPrime=" << HBarPrime;
    // ss << " deltahPrime=" << deltahPrime;
    // ss << " SsubL=" << SsubL;
    // ss << " SsubC=" << SsubC;
    // ss << " SsubH=" << SsubH;
    // ss << " RsubC=" << RsubC;
    // ss << " RsubT=" << RsubT;
    // ss << " g=" << g;
    // ss << " Tvar=" << Tvar;
    // ss << " deltaRO=" << deltaRO;
    // OutputDebugStringA(ss.str().data());

    return deltaE00;
}

float CIEdeltaE2000(double Cl_1, double Ca_1, double Cb_1, double Cl_2, double Ca_2, double Cb_2, float WHT_L, float WHT_C, float WHT_H) {
// Cl_1,  Ca_1,  Cb_1   - Color #1 CIE-L*ab values
// Cl_2,  Ca_2,  Cb_2   - Color #2 CIE-L*ab values
// WHT_L, WHT_C, WHT_H  - Weight factors: luminance, chroma and hue
// tested against http://www.brucelindbloom.com/index.html?ColorDifferenceCalc.html
// https://getreuer.info/posts/colorspace/index.html
// http://www.easyrgb.com/en/math.php
// https://zschuessler.github.io/DeltaE/demos/

  const double pwr = 6103515625; // 25^7;
  double xC1 = sqrt( Ca_1*Ca_1 + Cb_1*Cb_1 );
  double xC2 = sqrt( Ca_2*Ca_2 + Cb_2*Cb_2 );
  double xCX = ( xC1 + xC2 ) / 2.0;   // C-bar
  double zpx = xCX*xCX*xCX*xCX*xCX*xCX*xCX; // xCX^7
  double xGX = 0.5 * ( 1.0 - sqrt( zpx / ( zpx + pwr ) ) );

  double xNN = ( 1.0 + xGX ) * Ca_1;            // A-Prime 1
  xC1 = sqrt( xNN*xNN + Cb_1*Cb_1 );   // C-Prime 1
  double xH1 = CieLab2Hue( xNN, Cb_1 );       // H-Prime 1
 
  xNN = ( 1.0 + xGX ) * Ca_2;                   // A-Prime 2
  xC2 = sqrt( xNN*xNN + Cb_2*Cb_2 );   // C-Prime 2
  double xH2 = CieLab2Hue( xNN, Cb_2 );       // H-Prime 2

  // compute Delta H-Prime based on H-Primes
  double xDH; 
  if ( ( xC1 * xC2 ) == 0 )
  {
     xDH = 0;
  } else 
  {
     xNN = xH2 - xH1;   // the diff between the H-Primes
     if ( abs( xNN ) <= 180 ) {
        xDH = xNN;
     }
     else {
        if ( xNN > 180 )         // if (hPrime2 <= hPrime1) ???
           xDH = xNN - 360;
        else
           xDH = xNN + 360;
     }
  }

  xDH = 2 * sqrt( xC1 * xC2 ) * sin( deg2rad( xDH / 2 ) ); // Delta H-Prime

  double xHX;  // compute the H-Bar Prime
  if ( ( xC1 *  xC2 ) == 0 )
  {
     xHX = xH1 + xH2;
  } else
  {
     xNN = abs(xH1 - xH2);
     if ( xNN > 180 )
     {
        if ( ( xH2 + xH1 ) < 360 )
           xHX = xH1 + xH2 + 360;
        else
           xHX = xH1 + xH2 - 360;
     } else
     {
        xHX = xH1 + xH2;
     }
     xHX /= 2; // the H-Bar Prime
  }

  // xTX, the T variable, based on H-Bar Prime
  double xTX = 1.0 - 0.17 * cos( deg2rad(xHX - 30.0) ) + 0.24
                          * cos( deg2rad(2.0 * xHX ) ) + 0.32
                          * cos( deg2rad(3.0 * xHX + 6.0 ) ) - 0.20
                          * cos( deg2rad(4.0 * xHX - 63.0) );

  double xCY = ( xC1 + xC2 ) / 2.0;        // C-Bar Prime based on C-Primes
  double ytp = xCY*xCY*xCY*xCY*xCY*xCY*xCY; // xCY^7
  // compute R sub T
  double grp = ( xHX - 275.0 ) / 25.0;
  double xPH = 60.0 * exp(-1*(grp*grp));           // based on H-Bar Prime
  double xRC = 2.0 * sqrt( ytp / ( ytp + pwr ) );   // based on C-Bar Prime
  double xRT = - sin( deg2rad(xPH) ) * xRC;  // R sub T

  double xLX = ( Cl_1 + Cl_2 ) / 2.0 - 50.0; // L-Bar
  double xSL = 1.0 + ( 0.015 * (xLX*xLX) ) / sqrt( 20.0 + (xLX*xLX) );   // S sub L based on L-Bar
  double xSC = 1.0 + 0.045 * xCY;       // S sub C - based on C-Bar Prime
  double xSH = 1.0 + 0.015 * xCY * xTX; // S sub H - based on C-Bar Prime and T-var

  double xDL = Cl_2 - Cl_1;              // Delta L-Prime
  double xDC = xC2 - xC1;                // Delta C-Prime based on C-Primes
  xDL = xDL / ( WHT_L * xSL );
  xDC = xDC / ( WHT_C * xSC );
  xDH = xDH / ( WHT_H * xSH );

  double DeltaE = sqrt(xDL*xDL + xDC*xDC + xDH*xDH + xRT * xDC * xDH);
  return DeltaE;
}

auto RGBtoLAB(int sR, int sG, int sB) {
  // https://getreuer.info/posts/colorspace/index.html
  // http://www.easyrgb.com/en/math.php
  // sR, sG and sB (Standard RGB) input range = 0 ÷ 255
  // X, Y and Z outputs refer to a D65/2° standard illuminant.
  // https://zschuessler.github.io/DeltaE/demos/
  // tested against ColorMine.org

  // convert RGB to XYZ color space
  double var_R = char_to_float[sR];
  double var_G = char_to_float[sG];
  double var_B = char_to_float[sB];

  // Inverse sRGB gamma correction
  var_R = inverseGamma(var_R);
  var_G = inverseGamma(var_G);
  var_B = inverseGamma(var_B);

  var_R = var_R * 100;
  var_G = var_G * 100;
  var_B = var_B * 100;

  // compute XYZ color space values for given sRGB
  double X = var_R * 0.4123955889674142161 + var_G * 0.3575834307637148171 + var_B * 0.1804926473817015735;
  double Y = var_R * 0.2125862307855955516 + var_G * 0.7151703037034108499 + var_B * 0.07220049864333622685;
  double Z = var_R * 0.01929721549174694484 + var_G * 0.1191838645808485318 + var_B * 0.9504971251315797660;
  // std::stringstream ss;
  // ss << "qpv: color in XYZ - X=" << X;
  // ss << " Y=" << Y;
  // ss << " Z=" << Z;

  // compute XYZ according to the D65 reference illuminant specific to Daylight, sRGB and Adobe-RGB
  X /= 95.047;
  Y /= 100.000;
  Z /= 108.883;

  X = toLABf(X);
  Y = toLABf(Y);
  Z = toLABf(Z);

  std::array<double, 3> Lab;
  Lab[0] = 116*Y - 16;
  Lab[1] = 500*(X - Y);
  Lab[2] = 200*(Y - Z);

  // ss << " | in LAB - L=" << Lab[0];
  // ss << " a=" << Lab[1];
  // ss << " b=" << Lab[2];
  // OutputDebugStringA(ss.str().data());
  return Lab;
}

RGBColorI calculateBlendModes(int rO, int gO, int bO, int rB, int gB, int bB, int blendMode) {
    float rT, gT, bT;
    float rOf = char_to_float[rO];
    float gOf = char_to_float[gO];
    float bOf = char_to_float[bO];
    float rBf = char_to_float[rB];
    float gBf = char_to_float[gB];
    float bBf = char_to_float[bB];

    if (blendMode == 1) { // darken
        rT = min(rOf, rBf);
        gT = min(gOf, gBf);
        bT = min(bOf, bBf);
    }
    else if (blendMode == 2) { // multiply
        rT = rOf * rBf;
        gT = gOf * gBf;
        bT = bOf * bBf;
    }
    else if (blendMode == 3) { // linear burn
       rT = rOf + rBf - 1;
       gT = gOf + gBf - 1;
       bT = bOf + bBf - 1;
    }
    else if (blendMode == 4) { // color burn
       rT = 1 - ((1 - rBf) / rOf);
       gT = 1 - ((1 - gBf) / gOf);
       bT = 1 - ((1 - bBf) / bOf);
    }
    else if (blendMode == 5) { // lighten
        rT = max(rOf, rBf);
        gT = max(gOf, gBf);
        bT = max(bOf, bBf);
    }
    else if (blendMode == 6) { // screen
        rT = 1 - ( (1 - rBf) * (1 - rOf) );
        gT = 1 - ( (1 - gBf) * (1 - gOf) );
        bT = 1 - ( (1 - bBf) * (1 - bOf) );
    }
    else if (blendMode == 7) { // linear dodge [add]
        rT = rOf + rBf;
        gT = gOf + gBf;
        bT = bOf + bBf;
    }
    else if (blendMode == 8) { // hard light
        rT = (rOf < 0.5) ? 2 * rOf * rBf : 1 - (2 * (1 - rOf) * (1 - rBf) );
        gT = (gOf < 0.5) ? 2 * gOf * gBf : 1 - (2 * (1 - gOf) * (1 - gBf) );
        bT = (bOf < 0.5) ? 2 * bOf * bBf : 1 - (2 * (1 - bOf) * (1 - bBf) );
    }
    // else if (blendMode == 9) { // soft light A
    //     rT = (1 - 2*rOf) * pow(rBf, 2) + 2 * rOf * rBf;
    //     gT = (1 - 2*gOf) * pow(gBf, 2) + 2 * gOf * gBf;
    //     bT = (1 - 2*bOf) * pow(bBf, 2) + 2 * bOf * bBf;
    // }
    else if (blendMode == 9) { // soft light B
        rT = (rOf < 0.5) ? (1 - 2*rOf) * (rBf*rBf) + 2 * rBf * rOf : 2 * rBf * (1 - rOf) + sqrt(rBf) * (2 * rOf - 1);
        gT = (gOf < 0.5) ? (1 - 2*gOf) * (gBf*gBf) + 2 * gBf * gOf : 2 * gBf * (1 - gOf) + sqrt(gBf) * (2 * gOf - 1);
        bT = (bOf < 0.5) ? (1 - 2*bOf) * (bBf*bBf) + 2 * bBf * bOf : 2 * bBf * (1 - bOf) + sqrt(bBf) * (2 * bOf - 1);
    }
    else if (blendMode == 10) { // overlay
        rT = (rBf < 0.5) ? 2 * rOf * rBf : 1 - (2 * (1 - rOf) * (1 - rBf) );
        gT = (gBf < 0.5) ? 2 * gOf * gBf : 1 - (2 * (1 - gOf) * (1 - gBf) );
        bT = (bBf < 0.5) ? 2 * bOf * bBf : 1 - (2 * (1 - bOf) * (1 - bBf) );
    }
    else if (blendMode == 11) { // hard mix
        rT = (rOf <= (1 - rBf)) ? 0 : 1;
        gT = (gOf <= (1 - gBf)) ? 0 : 1;
        bT = (bOf <= (1 - bBf)) ? 0 : 1;
    }
    else if (blendMode == 12) { // linear light
        rT = rBf + (2 * rOf) - 1;
        gT = gBf + (2 * gOf) - 1;
        bT = bBf + (2 * bOf) - 1;
    }
    else if (blendMode == 13) { // color dodge
        rT = rBf / (1 - rOf);
        gT = gBf / (1 - gOf);
        bT = bBf / (1 - bOf);
    }
    else if (blendMode == 14) { // vivid light 
        // this blend mode combines Color Dodge and Color Burn (rescaled so that neutral colors become middle gray). Dodge applies when values in the top layer are lighter than middle gray, and burn applies to darker values
        if (rOf < 0.5)
           rT = 1 - (1 - rBf) / (2 * rOf);
        else
           rT = rBf / (2 * (1 - rOf));

        if (gOf < 0.5)
           gT = 1 - (1 - gBf) / (2 * gOf);
        else
           gT = gBf / (2 * (1 - gOf));

        if (bOf < 0.5)
           bT = 1 - (1 - bBf) / (2 * bOf);
        else
           bT = bBf / (2 * (1 - bOf));
    }
    else if (blendMode == 15) { // average
        rT = (rBf + rOf)/2;
        gT = (gBf + gOf)/2;
        bT = (bBf + bOf)/2;
    }
    else if (blendMode == 16) { // divide
        rT = rBf / rOf;
        gT = gBf / gOf;
        bT = bBf / bOf;
    }
    else if (blendMode == 17) { // exclusion
        rT = rOf + rBf - 2 * (rOf * rBf);
        gT = gOf + gBf - 2 * (gOf * gBf);
        bT = bOf + bBf - 2 * (bOf * bBf);
    }
    else if (blendMode == 18) { // difference
        rT = abs(rBf - rOf);
        gT = abs(gBf - gOf);
        bT = abs(bBf - bOf);
    }
    else if (blendMode == 19) { // substract
        rT = rBf - rOf;
        gT = gBf - gOf;
        bT = bBf - bOf;
    }
    else if (blendMode == 20) { // luminosity
        double lO = char_to_float[getGrayscale(rO, gO, bO)];
        double lB = char_to_float[getGrayscale(rB, gB, bB)];
        rT = lO + rBf - lB;
        gT = lO + gBf - lB;
        bT = lO + bBf - lB;

        // HSLColor hslO = ConvertRGBtoHSL(rOf, gOf, bOf, 1);
        // HSLColor hslB = ConvertRGBtoHSL(rBf, gBf, bBf, 1);
        // // RGBColor rgbf = ConvertHSLtoRGB(hslB.h, hslB.s, hslB.l);
        // RGBColor rgbf = ConvertHSLtoRGB(hslO.h, hslO.s, hslO.l);
        // rT = rgbf.r/255.0f;
        // gT = rgbf.g/255.0f;
        // bT = rgbf.b/255.0f;
    }
    else if (blendMode == 21) { // ghosting
        double lO = char_to_float[getGrayscale(rO, gO, bO)];
        double lB = char_to_float[getGrayscale(rB, gB, bB)];
        rT = lB - lO + rBf + rOf/5;
        gT = lB - lO + gBf + gOf/5;
        bT = lB - lO + bBf + bOf/5;
    }
    // else if (blendMode == 22) { // substract reverse
    //     rT = rOf - rBf;
    //     gT = gOf - gBf;
    //     bT = bOf - bBf;
    // }
    else if (blendMode == 22) { // inverted difference
        rT = (rOf > rBf) ? 1 - rOf - rBf : 1 - rBf - rOf;
        gT = (gOf > gBf) ? 1 - gOf - gBf : 1 - gBf - gOf;
        bT = (bOf > bBf) ? 1 - bOf - bBf : 1 - bBf - bOf;
    }

    rT = clamp(rT, 0.0f, 1.0f);
    gT = clamp(gT, 0.0f, 1.0f);
    bT = clamp(bT, 0.0f, 1.0f); 
    return {clamp((int)round(rT*255), 0, 255), clamp((int)round(gT*255), 0, 255), clamp((int)round(bT*255), 0, 255)};
}

RGBColorI NEWcalculateBlendModes(RGBAColor Orgb, RGBAColor Brgb, int blendMode, int flipLayers, int linearGamma) {
    // TO-DO this function must supersede/replace calculateBlendModes() used by ColourBrush() and FloodFill()
    float rT, gT, bT;
    if (blendMode==23)
    {
       int fR, fG, fB;
       float f = char_to_float[Orgb.a];
       if (linearGamma==1)
       {
          fR = linear_to_gamma[weighTwoValues(gamma_to_linear[Orgb.r], gamma_to_linear[Brgb.r], f)];
          fG = linear_to_gamma[weighTwoValues(gamma_to_linear[Orgb.g], gamma_to_linear[Brgb.g], f)];
          fB = linear_to_gamma[weighTwoValues(gamma_to_linear[Orgb.b], gamma_to_linear[Brgb.b], f)];
       } else
       {
          fR = weighTwoValues(Orgb.r, Brgb.r, f);
          fG = weighTwoValues(Orgb.g, Brgb.g, f);
          fB = weighTwoValues(Orgb.b, Brgb.b, f);
       }

       return {fR, fG, fB};
       // return {Orgb.r,Orgb.g,Orgb.b};
    }

    if (Orgb.a<1)
       return {Brgb.r, Brgb.g, Brgb.b};

    if (flipLayers==1)
       swap(Orgb, Brgb);

    float rOf = char_to_float[Orgb.r];
    float gOf = char_to_float[Orgb.g];
    float bOf = char_to_float[Orgb.b];
    float rBf = char_to_float[Brgb.r];
    float gBf = char_to_float[Brgb.g];
    float bBf = char_to_float[Brgb.b];

    if (blendMode == 1) { // darken
        rT = min(rOf, rBf);
        gT = min(gOf, gBf);
        bT = min(bOf, bBf);
    }
    else if (blendMode == 2) { // multiply
        rT = rOf * rBf;
        gT = gOf * gBf;
        bT = bOf * bBf;
    }
    else if (blendMode == 3) { // linear burn
       rT = rOf + rBf - 1;
       gT = gOf + gBf - 1;
       bT = bOf + bBf - 1;
    }
    else if (blendMode == 4) { // color burn
       rT = 1 - ((1 - rBf) / rOf);
       gT = 1 - ((1 - gBf) / gOf);
       bT = 1 - ((1 - bBf) / bOf);
    }
    else if (blendMode == 5) { // lighten
        rT = max(rOf, rBf);
        gT = max(gOf, gBf);
        bT = max(bOf, bBf);
    }
    else if (blendMode == 6) { // screen
        rT = 1 - ( (1 - rBf) * (1 - rOf) );
        gT = 1 - ( (1 - gBf) * (1 - gOf) );
        bT = 1 - ( (1 - bBf) * (1 - bOf) );
    }
    else if (blendMode == 7) { // linear dodge [add]
        rT = rOf + rBf;
        gT = gOf + gBf;
        bT = bOf + bBf;
    }
    else if (blendMode == 8) { // hard light
        rT = (rOf < 0.5) ? 2 * rOf * rBf : 1 - (2 * (1 - rOf) * (1 - rBf) );
        gT = (gOf < 0.5) ? 2 * gOf * gBf : 1 - (2 * (1 - gOf) * (1 - gBf) );
        bT = (bOf < 0.5) ? 2 * bOf * bBf : 1 - (2 * (1 - bOf) * (1 - bBf) );
    }
    // else if (blendMode == 9) { // soft light A
    //     rT = (1 - 2*rOf) * pow(rBf, 2) + 2 * rOf * rBf;
    //     gT = (1 - 2*gOf) * pow(gBf, 2) + 2 * gOf * gBf;
    //     bT = (1 - 2*bOf) * pow(bBf, 2) + 2 * bOf * bBf;
    // }
    else if (blendMode == 9) { // soft light B
        rT = (rOf < 0.5) ? (1 - 2*rOf) * (rBf*rBf) + 2 * rBf * rOf : 2 * rBf * (1 - rOf) + sqrt(rBf) * (2 * rOf - 1);
        gT = (gOf < 0.5) ? (1 - 2*gOf) * (gBf*gBf) + 2 * gBf * gOf : 2 * gBf * (1 - gOf) + sqrt(gBf) * (2 * gOf - 1);
        bT = (bOf < 0.5) ? (1 - 2*bOf) * (bBf*bBf) + 2 * bBf * bOf : 2 * bBf * (1 - bOf) + sqrt(bBf) * (2 * bOf - 1);
    }
    else if (blendMode == 10) { // overlay
        rT = (rBf < 0.5) ? 2 * rOf * rBf : 1 - (2 * (1 - rOf) * (1 - rBf) );
        gT = (gBf < 0.5) ? 2 * gOf * gBf : 1 - (2 * (1 - gOf) * (1 - gBf) );
        bT = (bBf < 0.5) ? 2 * bOf * bBf : 1 - (2 * (1 - bOf) * (1 - bBf) );
    }
    else if (blendMode == 11) { // hard mix
        rT = (rOf <= (1 - rBf)) ? 0 : 1;
        gT = (gOf <= (1 - gBf)) ? 0 : 1;
        bT = (bOf <= (1 - bBf)) ? 0 : 1;
    }
    else if (blendMode == 12) { // linear light
        rT = rBf + (2 * rOf) - 1;
        gT = gBf + (2 * gOf) - 1;
        bT = bBf + (2 * bOf) - 1;
    }
    else if (blendMode == 13) { // color dodge
        rT = rBf / (1 - rOf);
        gT = gBf / (1 - gOf);
        bT = bBf / (1 - bOf);
    }
    else if (blendMode == 14) { // vivid light 
        // this blend mode combines Color Dodge and Color Burn (rescaled so that neutral colors become middle gray). Dodge applies when values in the top layer are lighter than middle gray, and burn applies to darker values
        if (rOf < 0.5)
           rT = 1 - (1 - rBf) / (2 * rOf);
        else
           rT = rBf / (2 * (1 - rOf));

        if (gOf < 0.5)
           gT = 1 - (1 - gBf) / (2 * gOf);
        else
           gT = gBf / (2 * (1 - gOf));

        if (bOf < 0.5)
           bT = 1 - (1 - bBf) / (2 * bOf);
        else
           bT = bBf / (2 * (1 - bOf));
    }
    else if (blendMode == 15) { // average
        rT = (rBf + rOf)/2;
        gT = (gBf + gOf)/2;
        bT = (bBf + bOf)/2;
    }
    else if (blendMode == 16) { // divide
        rT = rBf / rOf;
        gT = gBf / gOf;
        bT = bBf / bOf;
    }
    else if (blendMode == 17) { // exclusion
        rT = rOf + rBf - 2 * (rOf * rBf);
        gT = gOf + gBf - 2 * (gOf * gBf);
        bT = bOf + bBf - 2 * (bOf * bBf);
    }
    else if (blendMode == 18) { // difference
        rT = abs(rBf - rOf);
        gT = abs(gBf - gOf);
        bT = abs(bBf - bOf);
    }
    else if (blendMode == 19) { // substract
        rT = rBf - rOf;
        gT = gBf - gOf;
        bT = bBf - bOf;
    }
    else if (blendMode == 20) { // luminosity
        double lO = char_to_float[getGrayscale(Orgb.r, Orgb.g, Orgb.b)];
        double lB = char_to_float[getGrayscale(Brgb.r, Brgb.g, Brgb.b)];
        rT = lO + rBf - lB;
        gT = lO + gBf - lB;
        bT = lO + bBf - lB;

        // HSLColor hslO = ConvertRGBtoHSL(rOf, gOf, bOf, 1);
        // HSLColor hslB = ConvertRGBtoHSL(rBf, gBf, bBf, 1);
        // // RGBColor rgbf = ConvertHSLtoRGB(hslB.h, hslB.s, hslB.l);
        // RGBColor rgbf = ConvertHSLtoRGB(hslO.h, hslO.s, hslO.l);
        // rT = rgbf.r/255.0f;
        // gT = rgbf.g/255.0f;
        // bT = rgbf.b/255.0f;
    }
    else if (blendMode == 21) { // ghosting
        double lO = char_to_float[getGrayscale(Orgb.r, Orgb.g, Orgb.b)];
        double lB = char_to_float[getGrayscale(Brgb.r, Brgb.g, Brgb.b)];
        rT = lB - lO + rBf + rOf/5;
        gT = lB - lO + gBf + gOf/5;
        bT = lB - lO + bBf + bOf/5;
    }
    // else if (blendMode == 22) { // substract reverse
    //     rT = rOf - rBf;
    //     gT = gOf - gBf;
    //     bT = bOf - bBf;
    // }
    else if (blendMode == 22) { // inverted difference
        rT = (rOf > rBf) ? 1 - rOf - rBf : 1 - rBf - rOf;
        gT = (gOf > gBf) ? 1 - gOf - gBf : 1 - gBf - gOf;
        bT = (bOf > bBf) ? 1 - bOf - bBf : 1 - bBf - bOf;
    }

    rT = clamp(rT, 0.0f, 1.0f);
    gT = clamp(gT, 0.0f, 1.0f);
    bT = clamp(bT, 0.0f, 1.0f); 
    if (flipLayers==1)
    {
       swap(Orgb.a, Brgb.a);
       swap(rBf, rOf);
       swap(gBf, gOf);
       swap(bBf, bOf);
    }

    float fintensity;
    if (Brgb.a>254)
    {
       fintensity = char_to_float[Orgb.a];
       rT = weighTwoValues(rT, rBf, fintensity, 1);
       gT = weighTwoValues(gT, gBf, fintensity, 1);
       bT = weighTwoValues(bT, bBf, fintensity, 1);
    } else if (Brgb.a<255 && Orgb.a>0)
    {
       fintensity = char_to_float[Brgb.a];
       if (Orgb.a<255 && Orgb.a>0)
       {
          float f = char_to_float[Orgb.a];
          rT = weighTwoValues(rT, rBf, f, 1);
          gT = weighTwoValues(gT, gBf, f, 1);
          bT = weighTwoValues(bT, bBf, f, 1);
       }
       rT = weighTwoValues(rT, rOf, fintensity, 1);
       gT = weighTwoValues(gT, gOf, fintensity, 1);
       bT = weighTwoValues(bT, bOf, fintensity, 1);
    }

    if (linearGamma==1)
    {
       rT = pow(rT, 0.6f);
       gT = pow(gT, 0.6f);
       bT = pow(bT, 0.6f);
    }

    return {clamp((int)round(rT*255), 0, 255), clamp((int)round(gT*255), 0, 255), clamp((int)round(bT*255), 0, 255)};
}

void toCMYK(float red, float green, float blue, float* cmyk) {
  float k = min(255-red, min(255-green,255-blue));
  float c = 255*(255-red-k)/(255-k); 
  float m = 255*(255-green-k)/(255-k); 
  float y = 255*(255-blue-k)/(255-k); 

  cmyk[0] = c;
  cmyk[1] = m;
  cmyk[2] = y;
  cmyk[3] = k;
}

void toRGB(float c, float m, float y, float k, float *rgb) {
  rgb[0] = -((c * (255.0 - k)) / 255.0 + k - 255.0);
  rgb[1] = -((m * (255.0 - k)) / 255.0 + k - 255.0);
  rgb[2] = -((y * (255.0 - k)) / 255.0 + k - 255.0);
}

int mixColorsFloodFill(int colorB, float *colorA, float f, int dynamicOpacity, int blendMode, float prevCLRindex, float tolerance, int alternateMode, float thisCLRindex, int linearGamma) {
// source https://stackoverflow.com/questions/10139833/adding-colours-colors-together-like-paint-blue-yellow-green-etc
// http://www.easyrgb.com/en/math.php
 
  int aB = (colorB >> 24) & 0xFF;
  int rB = (colorB >> 16) & 0xFF;
  int gB = (colorB >> 8) & 0xFF;
  int bB = colorB & 0xFF;
  int aO = colorA[0];
  int rO = colorA[1];
  int gO = colorA[2];
  int bO = colorA[3];

  int aBf, rBf, gBf, bBf, aOf, rOf, gOf, bOf;
  aBf = (linearGamma==1) ? gamma_to_linear[aB] : aB;
  rBf = (linearGamma==1) ? gamma_to_linear[rB] : rB;
  gBf = (linearGamma==1) ? gamma_to_linear[gB] : gB;
  bBf = (linearGamma==1) ? gamma_to_linear[bB] : bB;
  aOf = (linearGamma==1) ? gamma_to_linear[aO] : aO;

  float fz;
  if (dynamicOpacity==1)
  {
     // int thisCLRindex = float(rB*0.299 + gB*0.587 + bB*0.115);
     // float thisCLRindex = RGBtoGray(rB, gB, bB, alternateMode);
     if (alternateMode==3)
     {
        fz = (float)thisCLRindex/tolerance;
     } else 
     {
        float diffu = max(thisCLRindex, prevCLRindex) - min(thisCLRindex, prevCLRindex);
        fz = (float)diffu/tolerance;
     }
     f = f - fz;
     if (f<0)
        f = 0;
  }

  if (blendMode>0)
  {
     RGBColorI blended = calculateBlendModes(rO, gO, bO, rB, gB, bB, blendMode);
     rOf = (linearGamma==1) ? gamma_to_linear[blended.r] : blended.r;
     gOf = (linearGamma==1) ? gamma_to_linear[blended.g] : blended.g;
     bOf = (linearGamma==1) ? gamma_to_linear[blended.b] : blended.b;
  } else
  {
     rOf = (linearGamma==1) ? gamma_to_linear[rO] : rO;
     gOf = (linearGamma==1) ? gamma_to_linear[gO] : gO;
     bOf = (linearGamma==1) ? gamma_to_linear[bO] : bO;
  }

  int aT = weighTwoValues(aOf, aBf, f);
  int rT = weighTwoValues(rOf, rBf, f);
  int gT = weighTwoValues(gOf, gBf, f);
  int bT = weighTwoValues(bOf, bBf, f);
  if (linearGamma==1)
  {
     aT = linear_to_gamma[aT];
     rT = linear_to_gamma[rT];
     gT = linear_to_gamma[gT];
     bT = linear_to_gamma[bT];
  }

    // std::stringstream ss;
    // ss << "qpv: opacity = " << f;
    // ss << " rA=" << rA;
    // ss << "rB=" << rB;
    // ss << "rT=" << rT;
    // ss << " | gA=" << gA;
    // ss << "gB=" << gB;
    // ss << "gT=" << gT;
    // // ss << " r = " << result;
    // OutputDebugStringA(ss.str().data());

  return (aT << 24) | ((rT & 0xFF) << 16) | ((gT & 0xFF) << 8) | (bT & 0xFF);
}

int clrBrushMixColors(int colorB, float *colorA, float f, int blendMode, int linearGamma, int flipLayers) {
// source https://stackoverflow.com/questions/10139833/adding-colours-colors-together-like-paint-blue-yellow-green-etc
// http://www.easyrgb.com/en/math.php
 
  int aB = (colorB >> 24) & 0xFF;
  int rB = (colorB >> 16) & 0xFF;
  int gB = (colorB >> 8) & 0xFF;
  int bB = colorB & 0xFF;
  int aO = colorA[0];
  int rO = colorA[1];
  int gO = colorA[2];
  int bO = colorA[3];
  int aBf, rBf, gBf, bBf, aOf, rOf, gOf, bOf;
  aBf = (linearGamma==1) ? gamma_to_linear[aB] : aB;
  rBf = (linearGamma==1) ? gamma_to_linear[rB] : rB;
  gBf = (linearGamma==1) ? gamma_to_linear[gB] : gB;
  bBf = (linearGamma==1) ? gamma_to_linear[bB] : bB;
  aOf = (linearGamma==1) ? gamma_to_linear[aO] : aO;
  rOf = (linearGamma==1) ? gamma_to_linear[rO] : rO;
  gOf = (linearGamma==1) ? gamma_to_linear[gO] : gO;
  bOf = (linearGamma==1) ? gamma_to_linear[bO] : bO;

  if (blendMode>0)
  {
     RGBColorI blended;
     if (flipLayers==1)
        blended = calculateBlendModes(rB, gB, bB, rO, gO, bO, blendMode);
     else
        blended = calculateBlendModes(rO, gO, bO, rB, gB, bB, blendMode);

     rOf = (linearGamma==1) ? gamma_to_linear[blended.r] : blended.r;
     gOf = (linearGamma==1) ? gamma_to_linear[blended.g] : blended.g;
     bOf = (linearGamma==1) ? gamma_to_linear[blended.b] : blended.b;
  }

  int aT = weighTwoValues(aOf, aBf, f);
  int rT = weighTwoValues(rOf, rBf, f);
  int gT = weighTwoValues(gOf, gBf, f);
  int bT = weighTwoValues(bOf, bBf, f);
  if (linearGamma==1)
  {
     aT = linear_to_gamma[aT];
     rT = linear_to_gamma[rT];
     gT = linear_to_gamma[gT];
     bT = linear_to_gamma[bT];
  }

    // std::stringstream ss;
    // ss << "qpv: opacity = " << f;
    // ss << " rA=" << rA;
    // ss << "rB=" << rB;
    // ss << "rT=" << rT;
    // ss << " | gA=" << gA;
    // ss << "gB=" << gB;
    // ss << "gT=" << gT;
    // // ss << " r = " << result;
    // OutputDebugStringA(ss.str().data());

  return (aT << 24) | ((rT & 0xFF) << 16) | ((gT & 0xFF) << 8) | (bT & 0xFF);
}

DLL_API int DLL_CALLCONV prepareSelectionArea(int x1, int y1, int x2, int y2, int w, int h, float xf, float yf, float angle, int mode, int flip, float exclusion, int invertArea, float* PointsList, int PointsCount, int ppx1, int ppy1, int ppx2, int ppy2, int useCache, int ppofYa, int ppofYb) {
    imgSelX1 = x1;
    imgSelY1 = y1;
    imgSelX2 = x2;
    imgSelY2 = y2;
    imgSelW = w;
    imgSelH = h;
    imgSelExclX = w - (w*exclusion);
    imgSelExclY = h - (h*exclusion);
    imgSelExclW = (w - imgSelExclX*2) / 2.0f;
    imgSelExclH = (h - imgSelExclY*2) / 2.0f;
    imgSelXscale = xf;
    imgSelYscale = yf;
    hImgSelW = w / 2.0f;
    hImgSelH = h / 2.0f;
    EllipseSelectMode = mode;
    flippedSelection = flip;
    invertSelection = invertArea;
    excludeSelectScale = exclusion;
    vpSelRotation = (angle * M_PI) / 180.0f; // convert to radians
    cosVPselRotation = cos(vpSelRotation);
    sinVPselRotation = sin(vpSelRotation);
    polyX = ppx1;
    polyY = ppy1;
    polyW = ppx2 - ppx1;
    polyH = ppy2 - ppy1;
    polyOffYa = ppofYa;
    polyOffYb = ppofYb;
    if (polygonMaskMap.size()<2000 || polygonMapMin.size()<100)
       useCache = 0;

    if (mode==2 && PointsList!=NULL && useCache!=1 && polyW>1 && polyH>1)
       FillMaskPolygon(w, h, PointsList, PointsCount, ppx1, ppy1, ppx2, ppy2);
    else if (mode==2 && useCache!=1)
       EllipseSelectMode = 0;

    return 1;
}

bool decideColorsEqual(int newColor, int oldColor, float tolerance, float prevCLRindex, int alternateMode, float *nC, float& index) {
    // should use , CIEDE2000
    if (oldColor==newColor)
       return 1;
    else if (tolerance<1)
       return 0;

    // int aB = (newColor >> 24) & 0xFF;
    int rB = (newColor >> 16) & 0xFF;
    int gB = (newColor >> 8) & 0xFF;
    int bB = newColor & 0xFF;
    bool result;
    // float index;
    // int index = float(rB*0.299 + gB*0.587 + bB*0.115);
    if (alternateMode==3)
    {
       // auto LabA = RGBtoLAB(nC[1], nC[2], nC[3]);
       auto LabB = RGBtoLAB(rB, gB, bB);
       index = CIEdeltaE2000(nC[4], nC[5], nC[6], LabB[0], LabB[1], LabB[2], 1, 1, 1);
       result = (index<=tolerance) ? 1 : 0;
    } else
    {
       index = RGBtoGray(rB, gB, bB, alternateMode);
       result = inRange(index - tolerance, index + tolerance, prevCLRindex);
    }
    return result;
}

int wrapRGBtoGray(int color, int mode) {
    int rB = (color >> 16) & 0xFF;
    int gB = (color >> 8) & 0xFF;
    int bB = color & 0xFF;
    int index = RGBtoGray(rB, gB, bB, mode);
    return index;
}

int FloodFill8Stack(int *imageData, int w, int h, int x, int y, int newColor, float *nC, int oldColor, float tolerance, float prevCLRindex, float opacity, int dynamicOpacity, int blendMode, int cartoonMode, int alternateMode, int eightWay, int linearGamma) {
// based on https://lodev.org/cgtutor/floodfill.html
// by Lode Vandevenne

  if (newColor==oldColor)
     return 0; //avoid infinite loop

  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1}; // relative neighbor x coordinates
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1}; // relative neighbor y coordinates
  static const int gx[4] = {0, 1, 0, -1}; // relative neighbor x coordinates
  static const int gy[4] = {-1, 0, 1, 0}; // relative neighbor y coordinates

  UINT maxPixels = w*h + w;
  UINT loopsOccured = 0;
  UINT suchDeviations = 0;
  int suchAppliedDeviations = 0;
  std::vector<bool> pixelzMap(maxPixels, 0);
  std::vector<float> indexes(maxPixels, 0);
  std::stack<int> starkX;
  std::stack<int> starkY;

  int px = y * w + x;
  pixelzMap[px] = 1;
  starkX.push(x);
  starkY.push(y);
  int k = (eightWay==1) ? 8 : 4;
  float defIndex = (alternateMode==3) ? 0 : prevCLRindex;
  float index;
  while (starkX.size())
  {
     if (maxPixels<loopsOccured)
        break;

     loopsOccured++;
     int x = starkX.top();
     int y = starkY.top();
     starkX.pop();
     starkY.pop();
     // #pragma omp parallel for schedule(static) default(none) num_threads(3)
     for (int i = 0; i < k; i++)
     {
        int nx = (eightWay==1) ? x + dx[i] : x + gx[i] ;
        int ny = (eightWay==1) ? y + dy[i] : y + gy[i];

        if (nx>=0 && nx<w && ny>=0 && ny<h)
        {
           int tpx = ny * w + nx;
           if (pixelzMap[tpx]==1)
              continue;

           int thisColor = imageData[tpx];
           if (thisColor==oldColor)
           {
              pixelzMap[tpx] = 1;
              indexes[tpx] = defIndex;
              starkX.push(nx);
              starkY.push(ny);
           } else if (tolerance>0)
           {
              if (decideColorsEqual(thisColor, oldColor, tolerance, prevCLRindex, alternateMode, nC, index))
              {
                 pixelzMap[tpx] = 1;
                 indexes[tpx] = index;
                 // pixelzMap.insert( std::pair<int, bool>(tpx, 1) );
                 starkX.push(nx);
                 starkY.push(ny);
                 suchDeviations++;
              }
           }
        }
     }
  }

  // std::stringstream ss;
  // ss << "qpv: suchDeviations = " << suchDeviations;

  int thisColor = 0;
  for (std::size_t pix = 0; pix < pixelzMap.size(); ++pix)
  {
      if (pixelzMap[pix]!=1)
         continue;

      // std::cout << it->first << " => " << it->second << '\n';
      suchAppliedDeviations++;
      if (tolerance>0 && (opacity<1 || dynamicOpacity==1 || blendMode>0 || cartoonMode==1))
      {
         int prevColor = imageData[pix];
         if (cartoonMode==1)
            thisColor = oldColor;
         else
            thisColor = mixColorsFloodFill(prevColor, nC, opacity, dynamicOpacity, blendMode, prevCLRindex, tolerance, alternateMode, indexes[pix], linearGamma);

         imageData[pix] = thisColor;
      } else
      {
         imageData[pix] = newColor;   // second element , the colour, will be used to mix colours; to-do
      }
  }
  
  // ss << " suchAppliedDeviations = " << suchAppliedDeviations;
  // ss << " mapSize = " << pixelzMap.size();
  // OutputDebugStringA(ss.str().data());
  return suchAppliedDeviations;
}

int FloodFillScanlineStack(int *imageData, int w, int h, int x, int y, int newColor, int oldColor) {
// based on https://lodev.org/cgtutor/floodfill.html
// by Lode Vandevenne
  if (oldColor == newColor)
     return 0;

  int x1;
  bool spanAbove, spanBelow;
  UINT maxPixels = w*h + w;
  UINT loopsOccured = 0;

  // std::vector<int> stack;
  // push(stack, x, y);
  std::stack<int> starkX;
  std::stack<int> starkY;
  // std::vector<int> stack;
  starkX.push(x);
  starkY.push(y);
  while (starkX.size())
  {
    int x = starkX.top();
    int y = starkY.top();
    x1 = x;

    while (x1 >= 0 && imageData[y * w + x1] == oldColor)
    {
       x1--;
    }

    x1++;
    spanAbove = spanBelow = 0;
    starkX.pop();
    starkY.pop();

    while (x1 < w && imageData[y * w + x1] == oldColor)
    {
       imageData[y * w + x1] = newColor;
       if (maxPixels<loopsOccured)
          break;

       loopsOccured++;
       // int clrA = imageData[(y - 1) * w + x1];
       // int clrB = imageData[(y + 1) * w + x1];
       if (!spanAbove && y > 0 && imageData[(y - 1) * w + x1] == oldColor)
       // if (spanAbove==0 && y>0 && clrA==oldColor)
       {
          starkX.push(x1);
          starkY.push(y - 1);
          spanAbove = 1;
       } else if (spanAbove && y > 0 && imageData[(y - 1) * w + x1] != oldColor)
       // } else if (spanAbove==1 && y>0 && clrA!=oldColor)
       {
          spanAbove = 0;
       }

       if (!spanBelow && y < h - 1 && imageData[(y + 1) * w + x1] == oldColor)
       // if (spanBelow==0 && y<(h-1) && clrB==oldColor)
       {
          starkX.push(x1);
          starkY.push(y + 1);
          spanBelow = 1;
       // } else if (spanBelow==1 && y<(h-1) && clrB!=oldColor)
       } else if (spanBelow && y < h - 1 && imageData[(y + 1) * w + x1] != oldColor)
       {
          spanBelow = 0;
       }
       x1++;
    }
  }
  return loopsOccured;
}

int ReplaceGivenColor(int *imageData, int w, int h, int x, int y, int newColor, float *nC, int prevColor, float tolerance, float prevCLRindex, float opacity, int dynamicOpacity, int blendMode, int cartoonMode, int alternateMode, int linearGamma) {
    if ((x < 0) || (x >= (w-1)) || (y < 0) || (y >= (h-1)))  // out of bounds
       return 0;

    int loopsOccured = 0;
    #pragma omp parallel for schedule(static) default(none) // num_threads(3)
    for (int zx = 0; zx < w; zx++)
    {
        float index;
        int oldColor = prevColor;
        int thisColor = 0;
        for (int zy = 0; zy < h; zy++)
        {
            if (decideColorsEqual(imageData[zx + zy * w], prevColor, tolerance, prevCLRindex, alternateMode, nC, index))
            {
               if (tolerance>0 && (opacity<1 || dynamicOpacity==1 || blendMode>0 || cartoonMode==1))
               {
                  int prevColor = imageData[zx + zy * w];
                  if (cartoonMode==1)
                     thisColor = oldColor;
                  else
                     thisColor = mixColorsFloodFill(prevColor, nC, opacity, dynamicOpacity, blendMode, prevCLRindex, tolerance, alternateMode, index, linearGamma);
                  imageData[zx + zy * w] = thisColor;
               } else
               {
                  imageData[zx + zy * w] = newColor;
               }
               loopsOccured++;
            }
        }
    }
    return loopsOccured;
}

DLL_API int DLL_CALLCONV FloodFillWrapper(int *imageData, int modus, int w, int h, int x, int y, int newColor, int tolerance, int fillOpacity, int dynamicOpacity, int blendMode, int cartoonMode, int alternateMode, int eightWay, int linearGamma) {
    if ((x < 0) || (x >= (w-1)) || (y < 0) || (y >= (h-1)))  // out of bounds
       return 0;

    float toleranza = (alternateMode==3) ? (float)tolerance/10.0 + 1 : tolerance;
    int prevColor = imageData[x + y * w];
    int aB = (prevColor >> 24) & 0xFF;
    int rB = (prevColor >> 16) & 0xFF;
    int gB = (prevColor >> 8) & 0xFF;
    int bB = prevColor & 0xFF;
    float prevCLRindex = RGBtoGray(rB, gB, bB, alternateMode);

    float nC[7];
    nC[0] = (newColor >> 24) & 0xFF;
    nC[1] = (newColor >> 16) & 0xFF;
    nC[2] = (newColor >> 8) & 0xFF;
    nC[3] = newColor & 0xFF;

    auto LabA = RGBtoLAB(rB, gB, bB);
    nC[4] = LabA[0];
    nC[5] = LabA[1];
    nC[6] = LabA[2];

    // auto LabB = RGBtoLAB(rB, gB, bB);
    // float CIE = CIEdeltaE2000(LabA[0], LabA[1], LabA[2], LabB[0], LabB[1], LabB[2], 1, 1, 1);
    // float CIE2 = testCIEdeltaE2000(LabA[0], LabA[1], LabA[2], LabB[0], LabB[1], LabB[2], 1, 1, 1);

    float opacity = char_to_float[fillOpacity];
    if (tolerance==0 && (opacity<1 || blendMode>0))
       newColor = mixColorsFloodFill(prevColor, nC, opacity, 0, blendMode, 0, 0, 0, 0, linearGamma);

    int r;
    if (modus==1)
       r = ReplaceGivenColor(imageData, w, h, x, y, newColor, nC, prevColor, toleranza, prevCLRindex, opacity, dynamicOpacity, blendMode, cartoonMode, alternateMode, linearGamma);
    else if (toleranza>0)
       r = FloodFill8Stack(imageData, w, h, x, y, newColor, nC, prevColor, toleranza, prevCLRindex, opacity, dynamicOpacity, blendMode, cartoonMode, alternateMode, eightWay, linearGamma);
    else
       r = FloodFillScanlineStack(imageData, w, h, x, y, newColor, prevColor);

    // std::stringstream ss;
    // ss << "qpv: opacity = " << opacity;
    // ss << " newColor = " << newColor;
    // ss << " TolerMode = " << alternateMode;
    // // ss << " | CIE=" << CIE;
    // ss << " | L=" << nC[4];
    // ss << " | pix affected=" << r;
    // ss << " tolerance = " << toleranza;
    // ss << " clrIndex = " << prevCLRindex;
    // OutputDebugStringA(ss.str().data());

    return r;
}

DLL_API int DLL_CALLCONV autoCropAider(int* BitmapData, int Width, int Height, int adaptLevel, double threshold, double vTolrc, int whichLoop, int aaMode, int* fcoord) {
   int maxThresholdHitsW = round(Width*threshold) + 1;
   if (maxThresholdHitsW>floor(Width/2))
      maxThresholdHitsW = floor(Width/2);

   int maxThresholdHitsH = round(Height*threshold) + 1;
   if (maxThresholdHitsH>floor(Height/2))
      maxThresholdHitsH = floor(Height/2);

   if (threshold==0)
      maxThresholdHitsW = maxThresholdHitsH = 1;

   int clrPrimeA = BitmapData[0];
   int clrPrimeB = BitmapData[1];
   int clrPrimeC = BitmapData[Height];
   int prevR1 = wrapRGBtoGray(clrPrimeA, 1);
   int prevR2 = wrapRGBtoGray(clrPrimeB, 1);
   int prevR3 = wrapRGBtoGray(clrPrimeC, 1);
   int prevR4 = (prevR1 + prevR2 + prevR3)/3;
   int oprevR4 = prevR4;

   int ToleranceHits = 0;
   int loopDone = 0;
   int x = 0; int y = 0;
   if (whichLoop==1)
   {
      for (y = 0; y < Height; y++)
      {
         for (x = 0; x < Width; x++)
         {
            int clrR1 = BitmapData[x + (y * Width)];
            int R1 = wrapRGBtoGray(clrR1, 1);
            int d = abs(prevR4 - R1);

            if (aaMode==1)
            {
               if (inRange(d - adaptLevel, d + adaptLevel, vTolrc))
                  prevR4 = R1;

               // d = min(d, abs(oprevR4 - R1));
            }

            if (ToleranceHits<maxThresholdHitsW && d>vTolrc)
            {
               ToleranceHits++;
            } else if (d<=vTolrc)
            {
               d = 0;
            } else
            {
               loopDone = 1;
               break;
            }
            // fnOutputDebug(std::to_string(whichLoop) + " d" + std::to_string(d) + " R" + std::to_string(R1) + " H" + std::to_string(ToleranceHits) + " maxH" + std::to_string(maxThresholdHitsW) );
         }

         ToleranceHits = 0;
         if (loopDone==1)
         {
            *fcoord = y;
            break;
         }
      }
   } else if (whichLoop==2)
   {
      for (x = 0; x < Width; x++)
      {
         for (y = 0; y < Height; y++)
         {
            int clrR1 = BitmapData[x + (y * Width)];
            // int clrR1 = BitmapData[x * Height + y];
            int R1 = wrapRGBtoGray(clrR1, 1);
            int d = abs(prevR4 - R1);

            if (inRange(d - adaptLevel, d + adaptLevel, vTolrc) && aaMode==1)
               prevR4 = R1;

            if (ToleranceHits<maxThresholdHitsW && d>vTolrc)
            {
               ToleranceHits++;
            } else if (d<=vTolrc)
            {
               d = 0;
            } else
            {
               loopDone = 1;
               break;
            }
            // fnOutputDebug(std::to_string(whichLoop) + " d" + std::to_string(d) + " R" + std::to_string(R1) + " H" + std::to_string(ToleranceHits) + " maxH" + std::to_string(maxThresholdHitsW) );
         }

         ToleranceHits = 0;
         if (loopDone==1)
         {
            *fcoord = x;
            break;
         }
      }
   }
   // fnOutputDebug(std::to_string(whichLoop) + " fc" + std::to_string(*fcoord)  + " x" + std::to_string(x) + " y" + std::to_string(y) + " prev=" + std::to_string(prevR4) );

   return 1;
}

DLL_API int DLL_CALLCONV EraserBrush(int *imageData, int *maskData, int w, int h, int invertMask, int replaceMode, int levelAlpha, int *clonedData, int useClone) {

    // #pragma omp parallel for schedule(dynamic) default(none)
    for (int x = 0; x < w; x++)
    {
        // int px;
        for (int y = 0; y < h; y++)
        {
            const int px = x * h + y;
            int alpha2;
            int a = (imageData[px] >> 24) & 0xFF;
            int intensity = (maskData[px] >> 8) & 0xff;
            if (invertMask == 1)
               intensity = 255 - intensity;

            float fintensity = char_to_float[intensity];
            if (a==0)
               continue;

            if (replaceMode == 1)
               alpha2 = min(levelAlpha, a);
            else if (replaceMode == 2)
               alpha2 = max(0, (int)a - levelAlpha);
            else
               alpha2 = levelAlpha;

            alpha2 = (alpha2==a) ? a : ceil(alpha2*fintensity + a*max(0.0f, 1.0f - fintensity));  // Formula: A*w + B*(1 – w)
            // int haha = (alpha2!=a) ? 1 : 0;
            if (alpha2!=a)
            {
                if (useClone==1)
                   imageData[px] = (alpha2 << 24) | (clonedData[px] & 0x00ffffff);
                else
                   imageData[px] = (alpha2 << 24) | (imageData[px] & 0x00ffffff);
            }
            // std::stringstream ss;
            // ss << "qpv: alpha2 = " << alpha2;
            // ss << " var a = " << a;
            // ss << " var haha = " << haha;
            // OutputDebugStringA(ss.str().data());
        }
    }
 
    // fnOutputDebug("eraser alpha = " + std::to_string(levelAlpha));
    return 1;
}

DLL_API int DLL_CALLCONV ColourBrush(int *opacityImgData, int *imageData, int *maskData, int newColor, int w, int h, int invertMask, int replaceMode, int brushAlpha, int blendMode, int *clonedData, int useClone, int overDraw, int linearGamma, int wa, int ha, int offX, int offY, int flipLayers) {
// only works with 32-PARGB bitmaps 
    float nC[4];
    nC[0] = (replaceMode==1) ? brushAlpha : (newColor >> 24) & 0xFF;
    nC[1] = (newColor >> 16) & 0xFF;
    nC[2] = (newColor >> 8) & 0xFF;
    nC[3] = newColor & 0xFF;
    int apx = (w/2) * h + h/2;
    int centerLevel = (maskData[apx] >> 8) & 0xff;

    float pK[4];
    pK[0] = 255;
    pK[1] = centerLevel;
    pK[2] = centerLevel;
    pK[3] = centerLevel;
    float fr = 2.5f;
    if (overDraw==1 && blendMode>0)
    {
       brushAlpha = clamp(brushAlpha + 128, 0, 255);
       fr = 3.25f;
    }

    float fb = (255.0f - brushAlpha)/255.0f;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            int px = x * h + y;
            int BGRcolor = (overDraw==0) ? clonedData[px] : imageData[px];
            int intensity = (maskData[px] >> 8) & 0xff;
            int clipFlag = (opacityImgData[px] >> 24) & 0xff;
            if (invertMask==1)
               intensity = 255 - intensity;

            float fintensity = char_to_float[intensity];
            if (overDraw==1)
            {
               fintensity -= fb;
            } else
            {
               int altoAlpha = (opacityImgData[px] >> 8) & 0xff;
               int tL = weighTwoValues(255, altoAlpha, fintensity/fr);
               fintensity = char_to_float[tL] - fb;
               if (clipFlag<2)
                  tL = 0;

               if (overDraw==0 || blendMode>0)
                  opacityImgData[px] = (clipFlag << 24) | ((tL & 0xFF) << 16) | ((tL & 0xFF) << 8) | (tL & 0xFF);
            }
    
            if (fintensity<0 || clipFlag<2)
               fintensity = 0;

            imageData[px] = clrBrushMixColors(BGRcolor, nC, fintensity, blendMode, linearGamma, flipLayers);
        }
    }
 
    // fnOutputDebug("alles gut");
    return 1;
}


DLL_API int DLL_CALLCONV FillImageHoles(int *imageData, int w, int h, int newColor) {
    // fnOutputDebug("FillImageHoles newColor = " + std::to_string(newColor));
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            const int px = x * h + y;
            int a = (imageData[px] >> 24) & 0xFF;
            if (a<2)
               imageData[px] = newColor;
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV PrepareAlphaChannelBlur(int *imageData, int w, int h, int givenLevel, int fillMissingOnly, int threadz) {
/*
; this function fills / replaces black pixels [and with opacity 0] with surrounding colors
; this helps mitigate the dark hallows that emerge when applying blur on images with areas that are fully transparent 
; the function can also be used to specify an opacity/alpha level of the image
*/
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = 0; x < w; x++)
    {
        INT64 px, y = 0;
        int defaultColor = 0;
        UINT BGRcolor = imageData[x + y * w];
        if (BGRcolor!=0x0)
           defaultColor = BGRcolor & 0x00ffffff;

        for (y = 0; y < h; y++)
        {
            px = x + y * w;
            BGRcolor = imageData[px];
            if (BGRcolor==0x0 && defaultColor)
               imageData[px] = (givenLevel << 24) | defaultColor;
            else
               defaultColor = BGRcolor & 0x00ffffff;

            if (fillMissingOnly==0)
               imageData[px] = (givenLevel << 24) | (imageData[px] & 0x00ffffff);
        }
    }

    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = w - 1; x >= 0; x--)
    {
        int px, y = h - 1;
        int defaultColor = 0;
        UINT BGRcolor = imageData[x + y * w];
        if (BGRcolor!=0x0)
           defaultColor = BGRcolor & 0x00ffffff;

        for (y = h - 1; y >= 0; y--)
        {
            px = x + y * w;
            BGRcolor = imageData[px];
            if (BGRcolor==0x0 && defaultColor)
               imageData[px] = (givenLevel << 24) | defaultColor;
            else
               defaultColor = BGRcolor & 0x00ffffff;
        }
    }
    return 1;
}

/*
pBitmap and pBitmap2Blend must be the same width and height
and in 32-ARGB or 24-RGB format.
*/

DLL_API int DLL_CALLCONV BlendBitmaps(unsigned char* bgrImageData, unsigned char* otherData, int w, int h, int Stride, int bpp, int blendMode, int flipLayers, int faderMode, int keepAlpha, int linearGamma) {
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            int aO = 255;
            int aB = 255;
            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            if (bpp==32)
            {
               aB = bgrImageData[3 + o];
               aO = otherData[3 + o];
            }

            if (aO < 1 && aB < 1 || aB<2 && faderMode==2)
            {
               if (faderMode==2)
               {
                  // in this mode; the bgrImageData is actually otherData
                  if (bpp==32)
                     bgrImageData[3 + o] = otherData[3 + o];
                  bgrImageData[2 + o] = otherData[2 + o];
                  bgrImageData[1 + o] = otherData[1 + o];
                  bgrImageData[o] = otherData[o];
               }
               continue;
            }

            RGBAColor Brgb = {bgrImageData[o], bgrImageData[o + 1], bgrImageData[o + 2], aB};
            RGBAColor Orgb = {otherData[o], otherData[o + 1], otherData[o + 2], aO};
            if (faderMode==2)
               swap(Brgb, Orgb);

            RGBColorI blended;
            blended = NEWcalculateBlendModes(Orgb, Brgb, blendMode, flipLayers, linearGamma);
            if (bpp==32 && keepAlpha!=1 && blendMode!=23 || faderMode==1 && blendMode==23)
               bgrImageData[3 + o] = (faderMode==1) ? min(aO, aB) : max(aO, aB);
            else if (bpp==32 && faderMode==2 && keepAlpha==1)
               bgrImageData[3 + o] = otherData[3 + o];

            bgrImageData[2 + o] = blended.r;
            bgrImageData[1 + o] = blended.g;
            bgrImageData[o] = blended.b;
        }
    }

    return 1;
}


/*
pBitmap will be filled with a random generated noise
It must be in 32-ARGB format: PXF32ARGB - 0x26200A.
*/


DLL_API int DLL_CALLCONV GenerateRandomNoise(int* bgrImageData, int w, int h, int intensity, int doGrayScale, int threadz, int fillBgr) {
    // srand (time(NULL));
    // #pragma omp parallel for default(none) num_threads(threadz)
    time_t nTime;
    srand((unsigned) time(&nTime));
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            unsigned char aT = 255;
            unsigned char z = rand() % 101;
            if (z<intensity)
            {
               // unsigned char rT = 0;
               bgrImageData[x + (y * w)] = (fillBgr!=1) ? 0 : (255 << 24) | (0 << 16) | (0 << 8) | 0;
               continue;
            }

            if (doGrayScale!=1)
            {
               unsigned char rT = rand() % 256;
               unsigned char gT = rand() % 256;
               unsigned char bT = rand() % 256;
               bgrImageData[x + (y * w)] = (aT << 24) | (rT << 16) | (gT << 8) | bT;
            } else
            {
               unsigned char rT = rand() % 256;
               bgrImageData[x + (y * w)] = (aT << 24) | (rT << 16) | (rT << 8) | rT;
            }
        }
    }

    return 1;
}


DLL_API int DLL_CALLCONV getPBitmapistoInfos(Gdiplus::GpBitmap* pBitmap, int w, int h, UINT* resultsArray) {
// unused function
     UINT entries = 256;
     UINT elements[256];
     // Gdiplus::DllExports::GetHistogramSize(3, entries);
     Gdiplus::DllExports::GdipBitmapGetHistogram(pBitmap, Gdiplus::HistogramFormatR, entries, elements, NULL, NULL, NULL);
     
     int medianValue = -1;
     int peakPointK = -1;
     int minBrLvlK = -1;
     UINT minPointK = 0;
     UINT modePointV = 0;
     UINT modePointK = 0;
     UINT thisSum = 0;
     UINT sumTotalBr = 0;
     UINT pixRms = 0;
     UINT TotalPixelz = w*h;
     UINT pixMinu = TotalPixelz;

     for (int thisIndex = 0; thisIndex < 256; thisIndex++)
     {
        // fnOutputDebug("histo [" + to_string(i) +  "] = " + to_string(elements[i])) ;
        int nrPixelz = elements[thisIndex];
        if (nrPixelz>modePointV)
        {
           modePointV = nrPixelz;
           modePointK = thisIndex;
        }

        if (nrPixelz>0)
        {
           if (medianValue == -1)
           {
              thisSum += nrPixelz;
              if (thisSum>TotalPixelz/2)
                 medianValue = thisIndex;
           }

           sumTotalBr += nrPixelz * thisIndex;
           peakPointK = thisIndex;     // max range in histogram
           if (minBrLvlK == -1)
              minBrLvlK = thisIndex;   // min range in histogram
       
           if (nrPixelz<pixMinu)
           {
              pixMinu = nrPixelz;
              minPointK = thisIndex;
           }
        }

        pixRms += pow(nrPixelz, 2);       // root-mean square
     }

     UINT avgu = round((sumTotalBr/TotalPixelz - 1)/2);
     UINT rmsu = round(sqrt(pixRms / (peakPointK - minBrLvlK)));

     resultsArray[0] = avgu;
     resultsArray[1] = medianValue;
     resultsArray[2] = peakPointK;
     resultsArray[3] = minBrLvlK;
     resultsArray[4] = rmsu;
     resultsArray[5] = modePointK;
     resultsArray[6] = minPointK;
     // fnOutputDebug("histo avgu=" + to_string(avgu));
     // fnOutputDebug("histo medianValue=" + to_string(medianValue));
     // fnOutputDebug("histo peakPointK=" + to_string(peakPointK));
     // fnOutputDebug("histo minBrLvlK=" + to_string(minBrLvlK));
     // fnOutputDebug("histo rms=" + to_string(rmsu));
     // fnOutputDebug("histo modePointK=" + to_string(modePointK));
     // fnOutputDebug("histo minPointK=" + to_string(minPointK));
     return 1;
}

/*
Pixelate C/C++ Function by Tic and fixed by Fincs;
https://autohotkey.com/board/topic/29449-gdi-standard-library-145-by-tic/page-55
*/

DLL_API int DLL_CALLCONV PixelateBitmap(unsigned char* sBitmap, unsigned char* dBitmap, int w, int h, int Stride, int Size, int bpp) {
    int sA, sR, sG, sB;
    UINT o;
    for (int y1 = 0; y1 < h / Size; ++y1)
    {
        for (int x1 = 0; x1 < w / Size; ++x1)
        {
            sA = sR = sG = sB = 0;
            for (int y2 = 0; y2 < Size; ++y2)
            {
                for (int x2 = 0; x2 < Size; ++x2)
                {
                    o = (bpp/8) * (x2 + x1 * Size) + Stride * (y2 + y1 * Size);
                    if (bpp==32)
                       sA += sBitmap[3 + o];
                    sR += sBitmap[2 + o];
                    sG += sBitmap[1 + o];
                    sB += sBitmap[o];
                }
            }

            if (bpp==32)
               sA /= Size * Size;
            sR /= Size * Size;
            sG /= Size * Size;
            sB /= Size * Size;
            for (int y2 = 0; y2 < Size; ++y2)
            {
                for (int x2 = 0; x2 < Size; ++x2)
                {
                    o = (bpp/8) * (x2 + x1 * Size) + Stride * (y2 + y1 * Size);
                    if (bpp==32)
                       dBitmap[3 + o] = sA;
                    dBitmap[2 + o] = sR;
                    dBitmap[1 + o] = sG;
                    dBitmap[o] = sB;
                }
            }
        }

        if (w % Size != 0)
        {
            sA = sR = sG = sB = 0;
            for (int y2 = 0; y2 < Size; ++y2)
            {
                for (int x2 = 0; x2 < w % Size; ++x2)
                {
                    o = (bpp/8) * (x2 + (w / Size) * Size) + Stride * (y2 + y1 * Size);
                    if (bpp==32)
                       sA += sBitmap[3 + o];
                    sR += sBitmap[2 + o];
                    sG += sBitmap[1 + o];
                    sB += sBitmap[o];
                }
            }

            int tmp = (w % Size) * Size;
            if (bpp==32)
               sA = tmp ? (sA / tmp) : 0;
            sR = tmp ? (sR / tmp) : 0;
            sG = tmp ? (sG / tmp) : 0;
            sB = tmp ? (sB / tmp) : 0;
            for (int y2 = 0; y2 < Size; ++y2)
            {
                for (int x2 = 0; x2 < w % Size; ++x2)
                {
                    o = (bpp/8) * (x2 + (w / Size) * Size) + Stride * (y2 + y1 * Size);
                    if (bpp==32)
                       dBitmap[3 + o] = sA;
                    dBitmap[2 + o] = sR;
                    dBitmap[1 + o] = sG;
                    dBitmap[o] = sB;
                }
            }
        }
    }

    for (int x1 = 0; x1 < w / Size; ++x1)
    {
        sA = sR = sG = sB = 0;
        for (int y2 = 0; y2 < h % Size; ++y2)
        {
            for (int x2 = 0; x2 < Size; ++x2)
            {
                o = (bpp/8) * (x2 + x1 * Size) + Stride * (y2 + (h / Size) * Size);
                if (bpp==32)
                   sA += sBitmap[3 + o];
                sR += sBitmap[2 + o];
                sG += sBitmap[1 + o];
                sB += sBitmap[o];
            }
        }

        int tmp = Size * (h % Size);
        if (bpp==32)
           sA = tmp ? (sA / tmp) : 0;
        sR = tmp ? (sR / tmp) : 0;
        sG = tmp ? (sG / tmp) : 0;
        sB = tmp ? (sB / tmp) : 0;

        for (int y2 = 0; y2 < h % Size; ++y2)
        {
            for (int x2 = 0; x2 < Size; ++x2)
            {
                o = (bpp/8) * (x2 + x1 * Size) + Stride * (y2 + (h / Size) * Size);
                if (bpp==32)
                   dBitmap[3 + o] = sA;
                dBitmap[2 + o] = sR;
                dBitmap[1 + o] = sG;
                dBitmap[o] = sB;
            }
        }
    }

    sA = sR = sG = sB = 0;
    for (int y2 = 0; y2 < h % Size; ++y2)
    {
        for (int x2 = 0; x2 < w % Size; ++x2)
        {
            o = (bpp/8) * (x2 + (w / Size) * Size) + Stride * (y2 + (h / Size) * Size);
            if (bpp==32)
               sA += sBitmap[3 + o];
            sR += sBitmap[2 + o];
            sG += sBitmap[1 + o];
            sB += sBitmap[o];
        }
    }

    int tmp = (w % Size) * (h % Size);
    if (bpp==32)
       sA = tmp ? (sA / tmp) : 0;
    sR = tmp ? (sR / tmp) : 0;
    sG = tmp ? (sG / tmp) : 0;
    sB = tmp ? (sB / tmp) : 0;

    for (int y2 = 0; y2 < h % Size; ++y2)
    {
        for (int x2 = 0; x2 < w % Size; ++x2)
        {
            o = (bpp/8) * (x2 + (w / Size) * Size) + Stride * (y2 + (h / Size) * Size);
            if (bpp==32)
               dBitmap[3 + o] = sA;
            dBitmap[2 + o] = sR;
            dBitmap[1 + o] = sG;
            dBitmap[o] = sB;
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV ConvertToGrayScale(unsigned char *BitmapData, const int w, const int h, const int modus, const int intensity, const int Stride, const int bpp, unsigned char *maskBitmap, const int mStride) {
// NTSC // CCIR 601 luma RGB weights:
// r := 0.29970, g := 0.587130, b := 0.114180

    const float fintensity = intensity/100.0f;
    #pragma omp parallel for schedule(dynamic) default(none)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            int G;
            if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
               continue;

            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            int zR = BitmapData[2 + o];
            int zG = BitmapData[1 + o];
            int zB = BitmapData[o];
            if (modus==1)
            {
               G = BitmapData[2 + o]; // red
            } else if (modus==2)
            {
               G = BitmapData[1 + o]; // green
            } else if (modus==3)
            {
               G = BitmapData[o];     // blue
            } else if (modus==4 && bpp==32)
            {
               G = BitmapData[3 + o]; // alpha
            } else // if (modus==5)
            {
               G = clamp((int)round(char_to_grayRfloat[zR] + char_to_grayGfloat[zG] + char_to_grayBfloat[zB]), 0, 255);
            }

            BitmapData[2 + o] = weighTwoValues(G, zR, fintensity);
            BitmapData[1 + o] = weighTwoValues(G, zG, fintensity);
            BitmapData[o]     = weighTwoValues(G, zB, fintensity);
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV FillSelectArea(unsigned char *BitmapData, int w, int h, int Stride, int bpp, int color, int opacity, int eraser, int linearGamma, int blendMode, int flipLayers, unsigned char *maskBitmap, int mStride, unsigned char *colorBitmap, int gStride, int gBpp, int fillBehind, int opacityMultiplier, int keepAlpha) {
    // fnOutputDebug("FillSelectArea mStride=" + std::to_string(mStride));
    // fnOutputDebug("clipMaskFilter=zx=" + std::to_string(zx1) + "/" + std::to_string(zx2) + "=w=" + std::to_string(max(zx1, zx2) - min(zx1, zx2)));
    // fnOutputDebug("clipMaskFilter=zy=" + std::to_string(zy1) + "/" + std::to_string(zy2) + "=h=" + std::to_string(max(zy1, zy2) - min(zy1, zy2)));
    RGBAColor initialColor;
    initialColor.a = (color >> 24) & 0xFF;
    initialColor.r = (color >> 16) & 0xFF;
    initialColor.g = (color >> 8) & 0xFF;
    initialColor.b = color & 0xFF;
    float fi = char_to_float[opacity];
    const int bpc = bpp/8;
    const int gbpc = gBpp/8;
    const int bmpX = (imgSelX1<0 || invertSelection==1) ? 0 : imgSelX1;
    const int bmpY = (imgSelY1<0 || invertSelection==1) ? 0 : imgSelY1;
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(8)
    for (int x = 0; x < w; x++)
    {
        INT64 kx = (INT64)x * bpc;
        INT64 kzx = (INT64)(x - bmpX) * gbpc;
        for (int y = 0; y < h; y++)
        {
            float fintensity;
            RGBAColor newColor;
            RGBAColor userColor;
            int thisOpacity, oR, oG, oB;
            if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
               continue;

            // INT64 o = CalcPixOffset(x, y, Stride, bpp);
            INT64 o = (INT64)y * Stride + kx;
            if (colorBitmap!=NULL)
            {
               // INT64 oz = CalcPixOffset(x - zx1, y - zy1, gStride, 32);
               INT64 oz = (INT64)(y - bmpY) * gStride + kzx;
               thisOpacity = (gBpp==32) ? colorBitmap[3 + oz] : opacity;
               if (opacityMultiplier>0 && gBpp==32)
                  thisOpacity = clamp(thisOpacity + opacityMultiplier, 0, 255);

               userColor.a = thisOpacity;
               userColor.r = colorBitmap[2 + oz];
               userColor.g = colorBitmap[1 + oz];
               userColor.b = colorBitmap[oz];
               newColor = userColor;
               fintensity = char_to_float[thisOpacity];
            } else
            {
               fintensity = fi;
               thisOpacity = opacity;
               userColor = initialColor;
               newColor = initialColor;
            }

            int tA = 255;
            int oA = 255;
            if (bpp==32)
            {
               oA = (eraser==-1) ? 0 : BitmapData[3 + o];
               tA = weighTwoValues(newColor.a, oA, fintensity);
               tA = (colorBitmap!=NULL) ? max(tA, oA) : tA;
               BitmapData[3 + o] = tA;
            }

            if (eraser!=1 || bpp!=32)
            {
               oR = (eraser==-1) ? 0 : BitmapData[2 + o];
               oG = (eraser==-1) ? 0 : BitmapData[1 + o];
               oB = (eraser==-1) ? 0 : BitmapData[o];
            }

            if (eraser!=1 || bpp!=32)
            {
               if (blendMode>0 && eraser==0 && tA>0)
               {
                  RGBAColor Orgb = {userColor.b, userColor.g, userColor.r, userColor.a};
                  RGBAColor Brgb = {oB, oG, oR, oA};

                  RGBColorI blended;
                  blended = NEWcalculateBlendModes(Orgb, Brgb, blendMode, flipLayers, 0);
                  if (keepAlpha!=1 && blendMode!=23 && bpp==32 && fillBehind!=1)
                  {
                     tA = max(userColor.a, oA, tA);   
                     BitmapData[3 + o] = tA;
                  } else if ((keepAlpha==1 || blendMode==23) && bpp==32 && fillBehind!=1)
                     BitmapData[3 + o] = oA;

                  newColor.r = blended.r;
                  newColor.g = blended.g;
                  newColor.b = blended.b;
               }

               if (fillBehind==1)
               {
                  fintensity = char_to_float[oA];
                  swap(oR, newColor.r);
                  swap(oG, newColor.g);
                  swap(oB, newColor.b);
               }

               if (linearGamma==1 && eraser==0 && tA>0)
               {
                  BitmapData[2 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[newColor.r], gamma_to_linear[oR], fintensity)];
                  BitmapData[1 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[newColor.g], gamma_to_linear[oG], fintensity)];
                  BitmapData[o]     = linear_to_gamma[weighTwoValues(gamma_to_linear[newColor.b], gamma_to_linear[oB], fintensity)];
                } else
                {
                  BitmapData[2 + o] = weighTwoValues(newColor.r, oR, fintensity);
                  BitmapData[1 + o] = weighTwoValues(newColor.g, oG, fintensity);
                  BitmapData[o]     = weighTwoValues(newColor.b, oB, fintensity);
                }
            }
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV AdjustImageColorsPrecise(unsigned char *BitmapData, int w, int h, int Stride, int bpp, int opacity, int invertColors, int altSat, int saturation, int altBright, int brightness, int altContra, int contrast, int altHiLows, int shadows, int highs, int hue, int tintDegrees, int tintAmount, int altTint, int gamma, int rOffset, int gOffset, int bOffset, int aOffset, int rThreshold, int gThreshold, int bThreshold, int aThreshold, int seeThrough, int linearGamma, int noClamping, int whitePoint, int blackPoint, int noiseMode, unsigned char *maskBitmap, int mStride) {
    int bpc = bpp/8;
    if (opacity<2)
      return 1;

    if (gamma!=300)
    {
       double zamma = 1.0f / ((float)gamma/300.0f);
       for (int i = 0; i < 65536; i++) {
          LUTgamma[i] = gammaMathsInt16(i, zamma);
       }
    }

    if (altBright==0 && brightness<0)
    {
       double zamma = 1.0f / ((float)(77069.0f - brightness)/77069.0f);
       for (int i = 0; i < 65536; i++) {
          LUTgammaBright[i] = gammaMathsInt16(i, zamma);
       }
    }

    float fiBright = (brightness>0) ? brightness/32768.0f : -1*int_to_float[-1*brightness];
    if (brightness!=0 && altBright==1)
    {
       for (int i = 0; i < 65536; i++) {
           LUTbright[i] = brightMathsInt16(i, fiBright);
       }
    }

    float azx = (altHiLows==1) ? 25 : 95;
    float factorHiLows = (65536.5f * (azx + 65535.0f)) / (65535.0f * (65536.5f - azx));
    float fiShadows = (shadows>0) ? shadows/32768.0f : -1*int_to_float[-1*shadows];
    if (shadows!=0)
    {
       for (int i = 0; i < 65536; i++) {
           LUTshadows[i] = brightMathsInt16(i, fiShadows);
       }
    }

    float fiHighs = (highs>0) ? highs/32768.0f : -1*int_to_float[-1*highs];
    if (highs!=0)
    {
       for (int i = 0; i < 65536; i++) {
           LUThighs[i] = brightMathsInt16(i, fiHighs);
       }
    }

    float factorContrast = contrast/98302.0f;
    if (contrast>65525)
       contrast = 65525;
    float fiContra = (65536.5f * (contrast + 65535.0f)) / (65535.0f * (65536.5f - contrast));
    if (contrast!=0)
    {
       for (int i = 0; i < 65536; i++) {
           LUTcontra[i] = contraMathsInt16(i, fiContra, 32768);
       }
    }

    if (hue<0)
       hue += 360;
    if (tintDegrees<0)
       tintDegrees += 360;

    float saturateFactor = (saturation<0) ? (65535.0f - abs(saturation))/131070.0f : 0.5f + saturation/131070.0f;
    float fintensity = char_to_float[opacity];
    time_t nTime;
    srand((unsigned) time(&nTime));

    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(8)
    for (int x = 0; x < w; x++)
    {
        INT64 kx = (INT64)x * bpc;
        for (int y = 0; y < h; y++)
        {
            int oA = 255;
            int oR, oG, oB;
            if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
               continue;

            // INT64 o = CalcPixOffset(x, y, Stride, bpp);
            INT64 o = (INT64)y * Stride + kx;
            if (bpp==32)
            {
               oA = BitmapData[3 + o];
               if (oA==0 && altContra==0 && aOffset==0)
                  continue;
            }

            oR = BitmapData[2 + o];
            oG = BitmapData[1 + o];
            oB = BitmapData[o];
            RGBA16color pixel = {char_to_int[oB], char_to_int[oG], char_to_int[oR], char_to_int[oA]};
            if (invertColors==1)
               pixel.invert();
            if (gamma!=300)
               pixel.gamma(gamma, brightness, altBright, noClamping);
            if (shadows!=0 || highs!=0)
            {
               int gray = (noClamping==1) ? 0 : getInt16grayscale(pixel.r, pixel.g, pixel.b);
               if (shadows!=0)
                  pixel.shadows(shadows, altHiLows, linearGamma, gray, noClamping, fiShadows);
               if (highs!=0)
                  pixel.highlights(highs, altHiLows, linearGamma, factorHiLows, gray, noClamping, fiHighs);
            }
            if (aOffset!=0 || rOffset!=0 || gOffset!=0 || bOffset!=0)
               pixel.channelOffset(aOffset, rOffset, gOffset, bOffset, noClamping);
            if (brightness!=0)
               pixel.brightness(brightness, altBright, noClamping, fiBright);
            if (contrast!=0)
               pixel.contrast(contrast, altContra, linearGamma, factorContrast, noClamping, fiContra);
            if (noClamping==1)
            {
               // the other filters rely on clamped values
               pixel.r = clamp(pixel.r, 0, 65535);
               pixel.g = clamp(pixel.g, 0, 65535);
               pixel.b = clamp(pixel.b, 0, 65535);
            }

            if (hue!=0)
               pixel.hueRotate(hue, saturateFactor, altSat, saturation);
            if (saturation!=0)
               pixel.saturation(saturation, altSat, linearGamma, saturateFactor);
            if (blackPoint>0)
               pixel.blackPoint(blackPoint, noiseMode);
            if (whitePoint<65535)
               pixel.whitePoint(whitePoint, noiseMode);
            if (tintAmount>0)
               pixel.tint(tintDegrees, tintAmount, altTint, linearGamma);
            if (aThreshold>=0 || rThreshold>=0 || gThreshold>=0 || bThreshold>=0)
               pixel.threshold(aThreshold, rThreshold, gThreshold, bThreshold, seeThrough);

            if (linearGamma==1 && opacity<255)
            {
               if (bpp==32)
                  BitmapData[3 + o] = int_to_char[linear_to_gammaInt16[weighTwoValues(gamma_to_linearInt16[pixel.a], gamma_to_linearInt16[char_to_int[oA]], fintensity)]];
               BitmapData[2 + o] = int_to_char[linear_to_gammaInt16[weighTwoValues(gamma_to_linearInt16[pixel.r], gamma_to_linearInt16[char_to_int[oR]], fintensity)]];
               BitmapData[1 + o] = int_to_char[linear_to_gammaInt16[weighTwoValues(gamma_to_linearInt16[pixel.g], gamma_to_linearInt16[char_to_int[oG]], fintensity)]];
               BitmapData[o]     = int_to_char[linear_to_gammaInt16[weighTwoValues(gamma_to_linearInt16[pixel.b], gamma_to_linearInt16[char_to_int[oB]], fintensity)]];
            } else
            {
               if (bpp==32)
                  BitmapData[3 + o] = weighTwoValues(int_to_char[pixel.a], oA, fintensity);
               BitmapData[2 + o] = weighTwoValues(int_to_char[pixel.r], oR, fintensity);
               BitmapData[1 + o] = weighTwoValues(int_to_char[pixel.g], oG, fintensity);
               BitmapData[o]     = weighTwoValues(int_to_char[pixel.b], oB, fintensity);
            }
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV AdjustImageColors(unsigned char *BitmapData, int w, int h, int Stride, int bpp, int opacity, int invertColors, int altSat, int saturation, int altBright, int brightness, int altContra, int contrast, int altHiLows, int shadows, int highs, int hue, int tintDegrees, int tintAmount, int altTint, int gamma, int rOffset, int gOffset, int bOffset, int aOffset, int rThreshold, int gThreshold, int bThreshold, int aThreshold, int seeThrough, int linearGamma, int noClamping, int whitePoint, int blackPoint, int noiseMode, unsigned char *maskBitmap, int mStride) {
    int bpc = bpp/8;
    if (opacity<2)
      return 1;

    brightness = (brightness>0) ? int_to_char[brightness] : - int_to_char[abs(brightness)];
    contrast = (contrast>0) ? int_to_char[contrast] : - int_to_char[abs(contrast)];
    shadows = (shadows>0) ? int_to_char[shadows] : - int_to_char[abs(shadows)];
    highs = (highs>0) ? int_to_char[highs] : - int_to_char[abs(highs)];
    rOffset = (rOffset>0) ? int_to_char[rOffset] : - int_to_char[abs(rOffset)];
    gOffset = (gOffset>0) ? int_to_char[gOffset] : - int_to_char[abs(gOffset)];
    bOffset = (bOffset>0) ? int_to_char[bOffset] : - int_to_char[abs(bOffset)];
    aOffset = (aOffset>0) ? int_to_char[aOffset] : - int_to_char[abs(aOffset)];
    whitePoint = (whitePoint<65535) ? int_to_char[whitePoint] : 255;
    blackPoint = (blackPoint>0) ? int_to_char[blackPoint] : 0;
    if (rThreshold>0)
       rThreshold = (rThreshold>0) ? int_to_char[rThreshold] : - int_to_char[abs(rThreshold)];
    if (gThreshold>0)
       gThreshold = (gThreshold>0) ? int_to_char[gThreshold] : - int_to_char[abs(gThreshold)];
    if (bThreshold>0)
       bThreshold = (bThreshold>0) ? int_to_char[bThreshold] : - int_to_char[abs(bThreshold)];
    if (aThreshold>0)
       aThreshold = (aThreshold>0) ? int_to_char[aThreshold] : - int_to_char[abs(aThreshold)];

    if (gamma!=300)
    {
       double zamma = 1.0f / ((float)gamma/300.0f);
       for (int i = 0; i < 256; i++) {
          LUTgamma[i] = gammaMaths(i, zamma);
       }
    }

    if (altBright==0 && brightness<0)
    {
       double zamma = 1.0f / ((float)(300 - brightness)/300.0f);
       for (int i = 0; i < 256; i++) {
          LUTgammaBright[i] = gammaMaths(i, zamma);
       }
    }

    if (brightness!=0 && altBright==1)
    {
       float fi = (brightness>0) ? brightness/128.0f : -1*char_to_float[-1*brightness];
       for (int i = 0; i < 256; i++) {
           LUTbright[i] = brightMaths(i, fi);
       }
    }

    float azx = (altHiLows==1) ? 25 : 95;
    float factorHiLows = (259.0f * (azx + 255.0f)) / (255.0f * (259.0f - azx));
    if (shadows!=0)
    {
       float fi = (shadows>0) ? shadows/128.0f : -1*char_to_float[-1*shadows];
       for (int i = 0; i < 256; i++) {
           LUTshadows[i] = brightMaths(i, fi);
       }
    }

    if (highs!=0)
    {
       float fi = (highs>0) ? highs/128.0f : -1*char_to_float[-1*highs];
       for (int i = 0; i < 256; i++) {
           LUThighs[i] = brightMaths(i, fi);
       }
    }

    float factorContrast = contrast/383.0f;
    if (contrast!=0)
    {
       if (contrast<-230)
          contrast = -230;
       float mid = gamma_to_linear[128];
       float fintensity = (contrast<0) ? char_to_float[contrast + 255] : 0;
       float factor = (259.0f * (contrast + 255.0f)) / (255.0f * (259.0f - contrast));
  
       float fi = (contrast>0) ? char_to_float[contrast] : -1*char_to_float[-1*contrast];
       float ff = (1.01f * (fi + 1.0f)) / (1.0f * (1.01f - fi));
       for (int i = 0; i < 256; i++)
       {
           if (contrast<0)
              LUTcontra[i] = linear_to_gamma[weighTwoValues(gamma_to_linear[i], mid, fintensity)];
           else
              LUTcontra[i] = clamp( (float)ff * (char_to_float[i] - 0.5f)  + 0.5f, 0.0f, 1.0f)*255.0f;
           // else
           //    LUTcontra[i] = contraMaths(i, factor, 128);
       }
    }
    if (hue<0)
       hue += 360;
    if (tintDegrees<0)
       tintDegrees += 360;

    time_t nTime;
    srand((unsigned) time(&nTime));
    float saturateFactor = (saturation<0) ? (65535.0f - abs(saturation))/131070.0f : 0.5f + saturation/131070.0f;
    float fintensity = char_to_float[opacity];
    #pragma omp parallel for schedule(dynamic) default(none) // num_threads(8)
    for (int x = 0; x < w; x++)
    {
        INT64 kx = (INT64)x * bpc;
        for (int y = 0; y < h; y++)
        {
            int oA = 255;
            int oR, oG, oB;
            if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
               continue;

            // INT64 o = CalcPixOffset(x, y, Stride, bpp);
            INT64 o = (INT64)y * Stride + kx;
            if (bpp==32)
            {
               oA = BitmapData[3 + o];
               if (oA==0 && altContra==0 && aOffset==0)
                  continue;
            }

            oR = BitmapData[2 + o];
            oG = BitmapData[1 + o];
            oB = BitmapData[o];
            RGBAColor pixel = {oB, oG, oR, oA};
            if (invertColors==1)
               pixel.invert();
            if (gamma!=300)
               pixel.gamma();
            if (aOffset!=0 || rOffset!=0 || gOffset!=0 || bOffset!=0)
               pixel.channelOffset(aOffset, rOffset, gOffset, bOffset);
            if (shadows!=0 || highs!=0)
            {
               int gray = getGrayscale(oR, oG, oB);
               if (shadows!=0)
                  pixel.shadows(altHiLows, linearGamma, gray);
               if (highs!=0)
                  pixel.highlights(altHiLows, linearGamma, factorHiLows, gray);
            }
            if (brightness!=0)
               pixel.brightness(brightness, altBright);
            if (contrast!=0)
               pixel.contrast(contrast, altContra, linearGamma, factorContrast);
            if (hue!=0)
               pixel.hueRotate(hue, saturateFactor, altSat, saturation);
            if (saturation!=0)
               pixel.saturation(saturation, altSat, linearGamma, saturateFactor);
            if (blackPoint>0)
               pixel.blackPoint(blackPoint, noiseMode);
            if (whitePoint<255)
               pixel.whitePoint(whitePoint, noiseMode);
            if (tintAmount>0)
               pixel.tint(tintDegrees, tintAmount, altTint, linearGamma);
            if (aThreshold>=0 || rThreshold>=0 || gThreshold>=0 || bThreshold>=0)
               pixel.threshold(aThreshold, rThreshold, gThreshold, bThreshold, seeThrough);

            if (linearGamma==1 && fintensity<1)
            {
               if (bpp==32)
                  BitmapData[3 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[pixel.a], gamma_to_linear[oA], fintensity)];
               BitmapData[2 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[pixel.r], gamma_to_linear[oR], fintensity)];
               BitmapData[1 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[pixel.g], gamma_to_linear[oG], fintensity)];
               BitmapData[o]     = linear_to_gamma[weighTwoValues(gamma_to_linear[pixel.b], gamma_to_linear[oB], fintensity)];
            } else
            {
               if (bpp==32)
                  BitmapData[3 + o] = weighTwoValues(pixel.a, oA, fintensity);
               BitmapData[2 + o] = weighTwoValues(pixel.r, oR, fintensity);
               BitmapData[1 + o] = weighTwoValues(pixel.g, oG, fintensity);
               BitmapData[o]     = weighTwoValues(pixel.b, oB, fintensity);
            }
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV MergeBitmapsWithMask(unsigned char *originalData, unsigned char *newBitmap, unsigned char *maskBitmap, int invert, int w, int h, int maskOpacity, int invertMaskOpacity, int Stride, int bpp, int linearGamma, int whichChannel) {
    #pragma omp parallel for schedule(dynamic) default(none)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            int nA = 1;
            int oA = 1;
            int intensity = 0;
            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            if (maskBitmap!=NULL)
            {
               intensity = (invert==1) ? 255 - maskBitmap[o + whichChannel] : maskBitmap[o + whichChannel];
               if (maskOpacity!=0)
               {
                  intensity = (invertMaskOpacity==1) ? intensity + maskOpacity : intensity - maskOpacity;
                  intensity = clamp(intensity, 0, 255);
               }
            } else intensity = maskOpacity;

            if (intensity<1)
               continue;

            if (bpp==32)
               nA = newBitmap[3 + o];
            int nR = newBitmap[2 + o];
            int nG = newBitmap[1 + o];
            int nB = newBitmap[o];
            if (intensity>254)
            {
               originalData[2 + o] = nR;
               originalData[1 + o] = nG;
               originalData[o] = nB;
               if (bpp==32)
                  originalData[3 + o] = nA;
               continue;
            }

            if (bpp==32)
               oA = originalData[3 + o];
            int oR = originalData[2 + o];
            int oG = originalData[1 + o];
            int oB = originalData[o];
            float fintensity = char_to_float[intensity];
            if (linearGamma==1)
            {
               originalData[2 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[nR], gamma_to_linear[oR], fintensity)];
               originalData[1 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[nG], gamma_to_linear[oG], fintensity)];
               originalData[o]     = linear_to_gamma[weighTwoValues(gamma_to_linear[nB], gamma_to_linear[oB], fintensity)];
            } else
            {
               originalData[2 + o] = weighTwoValues(nR, oR, fintensity);
               originalData[1 + o] = weighTwoValues(nG, oG, fintensity);
               originalData[o]     = weighTwoValues(nB, oB, fintensity);
            }

            if (bpp==32)
               originalData[3 + o] = weighTwoValues(nA, oA, fintensity);
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV PixelateHugeBitmap(unsigned char *originalData, int w, int h, int Stride, int bpp, int maskOpacity, int blendMode, int flipLayers, int keepAlpha, int linearGamma, unsigned char *newBitmap, int StrideMini, int mw, int mh, int bImgSelW, int bImgSelH, unsigned char *maskBitmap, int mStride) {
    if (maskOpacity<2)
       return 1;

    std::vector<int> pixelzMapW(w, 0);
    std::vector<int> pixelzMapH(h, 0);
    const int bmpX = (imgSelX1<0 || invertSelection==1) ? 0 : imgSelX1;
    const int bmpY = (imgSelY1<0 || invertSelection==1) ? 0 : imgSelY1;
    for (int x = 0; x < w; x++)
    {
        pixelzMapW[x] = (float)mw*((x - bmpX)/(float)bImgSelW);
    }

    for (int y = 0; y < h; y++)
    {
        pixelzMapH[y] = (float)mh*((y - bmpY)/(float)bImgSelH);
    }

    const float fintensity = char_to_float[maskOpacity];
    #pragma omp parallel for schedule(dynamic) default(none)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            int nA = 255;
            int oA = 255;
            if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
               continue;

            INT64 on = CalcPixOffset(pixelzMapW[x], pixelzMapH[y], StrideMini, bpp);
            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            if (bpp==32)
               nA = newBitmap[3 + on];
            int nR = newBitmap[2 + on];
            int nG = newBitmap[1 + on];
            int nB = newBitmap[on];
 
            if (bpp==32)
               oA = originalData[3 + o];
            int oR = originalData[2 + o];
            int oG = originalData[1 + o];
            int oB = originalData[o];

            if (blendMode>0)
            {
               RGBAColor Orgb = {nB, nG, nR, nA};
               RGBAColor Brgb = {oB, oG, oR, oA};   

               RGBColorI blended;
               blended = NEWcalculateBlendModes(Orgb, Brgb, blendMode, flipLayers, 0);
               if (keepAlpha!=1 && blendMode!=23 && bpp==32)
                  nA = max(nA, oA);

               nR = blended.r;
               nG = blended.g;
               nB = blended.b;
            }

            if (linearGamma==1)
            {
               originalData[2 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[nR], gamma_to_linear[oR], fintensity)];
               originalData[1 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[nG], gamma_to_linear[oG], fintensity)];
               originalData[o]     = linear_to_gamma[weighTwoValues(gamma_to_linear[nB], gamma_to_linear[oB], fintensity)];
            } else
            {
               originalData[2 + o] = weighTwoValues(nR, oR, fintensity);
               originalData[1 + o] = weighTwoValues(nG, oG, fintensity);
               originalData[o]     = weighTwoValues(nB, oB, fintensity);
            }

            if (bpp==32)
               originalData[3 + o] = weighTwoValues(nA, oA, fintensity);
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV UndoAiderSwapPixelRegions(unsigned char* BitmapData, int w, int h, const INT64 Stride, unsigned char* otherData, const INT64 mStride, int bpp, const int x1, const int y1, const int x2, const int y2) {

    const INT64 bytesPerPixel = (bpp==32) ? 4 : 3;
    const INT64 opx = x1 * bytesPerPixel;
    const INT64 chunk = (x2 - x1) * bytesPerPixel;
    #pragma omp parallel for schedule(dynamic) default(none)
    for (int y = y1; y < y2; y++)
    {
            INT64 o = y * Stride + opx;
            INT64 n = (y - y1) * mStride;
            // Swap pixels
            BYTE *temp = NULL;
            temp = new BYTE[chunk];
            memcpy(temp, &BitmapData[o], chunk);
            memcpy(&BitmapData[o], &otherData[n], chunk);
            memcpy(&otherData[n], temp, chunk);
            delete[] temp;
    }
    return 1;
}

DLL_API int DLL_CALLCONV ColorizeGrayImage(unsigned char *originalData, int w, int h, int Stride, int bpp, int linearGamma, int colorA, int colorB) {
    const int aB = (colorB >> 24) & 0xFF;
    const int rB = (colorB >> 16) & 0xFF;
    const int gB = (colorB >> 8) & 0xFF;
    const int bB = colorB & 0xFF;
    const int aA = (colorA >> 24) & 0xFF;
    const int rA = (colorA >> 16) & 0xFF;
    const int gA = (colorA >> 8) & 0xFF;
    const int bA = colorA & 0xFF;

    #pragma omp parallel for schedule(dynamic) default(none)
    for (int x = 0; x < w; x++)
    {
        for (int y = 0; y < h; y++)
        {
            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            float fintensity = char_to_float[originalData[o]];
            // float f = char_to_float[originalData[o]];
            if (linearGamma==1)
            {
               originalData[2 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[rA], gamma_to_linear[rB], fintensity)];
               originalData[1 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[gA], gamma_to_linear[gB], fintensity)];
               originalData[o]     = linear_to_gamma[weighTwoValues(gamma_to_linear[bA], gamma_to_linear[bB], fintensity)];
            } else
            {
               originalData[2 + o] = weighTwoValues(rA, rB, fintensity);
               originalData[1 + o] = weighTwoValues(gA, gB, fintensity);
               originalData[o]     = weighTwoValues(bA, bB, fintensity);
            }

            if (bpp==32)
               originalData[3 + o] = weighTwoValues(aA, aB, fintensity);
        }
    }
    return 1;
}

inline int hammingDistance(UINT64 n1, UINT64 n2, UINT hamDistLBorderCrop, UINT hamDistRBorderCrop, bool doRange) { 
    UINT64 x = n1 ^ n2; 
    int setBits = 0; 
    if (doRange==0)
    {
        while (x > 0)
        { 
           setBits += x & 1; 
           x >>= 1; 
        }
    } else
    {
        int loopsOccured = 0;
        while (x > 0)
        { 
           loopsOccured++;
           if (inRange(hamDistLBorderCrop, 64 - hamDistRBorderCrop, loopsOccured))
              setBits += x & 1; 
           x >>= 1; 
        }
    }  

    return setBits; 
} 

DLL_API UINT DLL_CALLCONV retrieveHammingDistanceResults(UINT *resultsArray, UINT whichArray, UINT results) {
   for ( int index = 0 ; index <= results ; index++)
   {
        if (whichArray==1)
           resultsArray[index] = dupesListIDsA[index];
        else if (whichArray==2)
           resultsArray[index] = dupesListIDsB[index];
        else if (whichArray==3)
           resultsArray[index] = dupesListIDsC[index];
   }

   if (whichArray==1)
   {
      // dupesListIDsA.clear();
      dupesListIDsA.resize(1);
   } else if (whichArray==2)
   {
      // dupesListIDsB.clear();
      dupesListIDsB.resize(1);
   } else if (whichArray==3)
   {
      // dupesListIDsC.clear();
      dupesListIDsC.resize(1);
   }

   return 1;
}

DLL_API UINT DLL_CALLCONV clearHammingDistanceResults() {
    // dupesListIDsA.clear();
    dupesListIDsA.resize(1);
    // dupesListIDsB.clear();
    dupesListIDsB.resize(1);
    // dupesListIDsC.clear();
    dupesListIDsC.resize(1);
    dupesListIDsA.shrink_to_fit();
    dupesListIDsB.shrink_to_fit();
    dupesListIDsC.shrink_to_fit();

    return 1;
}

void setMainWindowTitle(std::string str, HWND pvHwnd) {
  // std::string str = "Calculating hamming distance: " + std::to_string(yay) + " / " + std::to_string(yoyo);
  std::wstring temp = std::wstring(str.begin(), str.end());
  LPCWSTR wideString = temp.c_str();

  SetWindowText(pvHwnd, wideString);
}

DLL_API UINT DLL_CALLCONV hammingDistanceOverArray(UINT64 *givenHashesArray, UINT64 *givenFlippedHashesArray, UINT *givenIDs, UINT arraySize, int threshold, UINT hamDistLBorderCrop, UINT hamDistRBorderCrop, int checkInverted, int checkFlipped, int stepping, int offsetu, int* hoffset) {
   UINT results = 0;
   UINT n = arraySize;
   bool doRange = (hamDistLBorderCrop==0 && hamDistRBorderCrop==0) ? 0 : 1;
   // int mainIndex = 1;
   // int returnVal = 1;
   // std::stringstream ss;
   // ss << "qpv: arraySize " << n;
   // ss << " maxResults " << maxResults;
   // OutputDebugStringA(ss.str().data());

    int noffset = 0;
    for ( INT secondIndex = offsetu ; secondIndex<n+1 ; secondIndex++)
    {
        UINT64 invert2ndindex = 0;
        // UINT64 reversed2ndindex = 0;
        if (checkInverted==1)
           invert2ndindex = ~(givenHashesArray[secondIndex]);

        // if (checkFlipped==1)
        // {
        //    reversed2ndindex = revBits_entire(givenHashesArray[secondIndex]);
        //    // fnOutputDebug("reverso " + to_string(givenHashesArray[secondIndex]) + " -- " + to_string(reversed2ndindex));
        // }

        noffset++;
        #pragma omp parallel for schedule(dynamic) default(none) shared(results)
        for ( INT mainIndex = secondIndex + 1 ; mainIndex<n ; mainIndex++)
        {
            int diff2 = 900;
            int diff3 = 900;

            int diff = hammingDistance(givenHashesArray[mainIndex], givenHashesArray[secondIndex], hamDistLBorderCrop, hamDistRBorderCrop, doRange);
            if (checkInverted==1)
               diff2 = hammingDistance(givenHashesArray[mainIndex], invert2ndindex, hamDistLBorderCrop, hamDistRBorderCrop, doRange);
            if (checkFlipped==1)
               diff3 = hammingDistance(givenHashesArray[mainIndex], givenFlippedHashesArray[secondIndex], hamDistLBorderCrop, hamDistRBorderCrop, doRange);

            // if (threshold>2 && diff>=threshold)
            //    diff = hammingDistance(givenHashesArray[mainIndex], reversed2ndindex, hamDistLBorderCrop, hamDistRBorderCrop);

            #pragma omp critical
            {
               if (diff<threshold)
               {
                   dupesListIDsA.push_back(givenIDs[mainIndex]);
                   dupesListIDsB.push_back(givenIDs[secondIndex]);
                   dupesListIDsC.push_back(diff);
                   results++;
               };

               if (diff2<threshold)
               {
                   dupesListIDsA.push_back(givenIDs[mainIndex]);
                   dupesListIDsB.push_back(givenIDs[secondIndex]);
                   dupesListIDsC.push_back(diff2);
                   results++;
               }

               if (diff3<threshold)
               {
                   // fnOutputDebug("c++ dupe pair:" + to_string(givenHashesArray[mainIndex]) + "/" + to_string(givenFlippedHashesArray[secondIndex]));
                   dupesListIDsA.push_back(givenIDs[mainIndex]);
                   dupesListIDsB.push_back(givenIDs[secondIndex]);
                   dupesListIDsC.push_back(diff3);
                   results++;
               }
            }
        }
        if (noffset>stepping)
           break;
    }

   *hoffset = noffset;
   // fnOutputDebug("hamDist results=" + to_string(results));
   // int test = hammingDistance(givenHashesArray[5], givenHashesArray[7]);
   // std::stringstream ss;
   // ss << "qpv: hashes results " << results;
   // ss << " hA " << givenHashesArray[5];
   // ss << " hB " << givenHashesArray[7];
   // ss << " idA " << givenIDs[5];
   // ss << " idB " << givenIDs[7];
   // ss << " diff " << test;
   // OutputDebugStringA(ss.str().data());
   return results;
}

double calcArrayAvgMedian(std::array<double, 64> givenArray, int modus) {
    int size = givenArray.size() - 1;
    if (modus==1) // median
    {
        std::sort(givenArray.begin(), givenArray.end());
        if (size % 2 == 0) {
            return (givenArray[round(size / 2 - 1)] + givenArray[round(size / 2)]) / 2;
        }

        return givenArray[floor(size / 2)];
    } else
    {
        // Calculate the average value from top 8x8 pixels, except for the first one.
        double thisSum = 0;
        for (int i = 0; i < size; i++)
        {
            if (i > 0)
            {
                thisSum += givenArray[i];
            }
        }

        return (thisSum / size);
    }
}

DLL_API int DLL_CALLCONV calculateDCTcoeffs(int size) {
    int thisIndex = 0;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++) {
            thisIndex++;
            DCTcoeffs[thisIndex] = cos(i * M_PI * (j + 0.5) / size);
        }
    }

    return 1;
}

auto calculateDCT(const std::array<double, 32> &matrix, int col, int loopu) {
    // int size = 32;
    std::array<double, 32> transformed;
    // double div2sz = sqrt(2.0 / size);
    // double div2sq = 1 / sqrt(2.0);

    int thisIndex = 0;
    for (int i = 0; i < 32; i++)
    {
        double sum = 0.0;
        for (int j = 0; j < 32; j++) {
            thisIndex++;
            sum += matrix[j] * DCTcoeffs[thisIndex];
            // sum += matrix[j] * cos(i * M_PI * (j + 0.5) / 32);
        }

        sum *= div2sz;
        if (i == 0) {
            sum *= div2sq;
        }

        transformed[i] = sum;
        // fnOutputDebug("calcPHashAlgo: col=" + to_string(col) + " loopu=" + to_string(loopu) + " matrix[" + to_string(i) + "] DCT=" + to_string(matrix[i]));
    }

    return transformed;
}


DLL_API INT64 DLL_CALLCONV calcPHashAlgo(char *givenArray, UINT size, int compareMethod) {
// based on the PHP implementation found on https://github.com/jenssegers/imagehash

    // givenArray is the pixels fingerprint
    // calculte DCT for rows

    double rows[32][32];
    std::array<double, 32>  trow;
    // fnOutputDebug("calcPHashAlgo: init before loop" + to_string(size));
    for ( int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++) 
        {
            trow[x] = abs(givenArray[x + size*y]);
        }

        auto transformed = calculateDCT(trow, y, 1);
        for (int x = 0; x < size; x++) 
        {
            rows[y][x] = transformed[x];
            // fnOutputDebug("calcPHashAlgo: row DCT=" + to_string(rows[y][x]));
        }
    }

    // fnOutputDebug("calcPHashAlgo: first for-loop" + to_string(rows[12][13]));
    // fnOutputDebug("calcPHashAlgo: trow" + to_string(trow[12]));
    // fnOutputDebug("calcPHashAlgo: givenArray" + to_string(givenArray[12]));

    // calculte DCT for columns
    double matrix[32][32];
    std::array<double, 32>  col;
    for (int x = 0; x < size; x++)
    {
        for (int y = 0; y < size; y++)
        {
            col[y] = rows[y][x];
        }

        auto transformed = calculateDCT(col, x, 2);
        for (int y = 0; y < size; y++)
        {
            matrix[x][y] = transformed[y];
        }
    }
    // fnOutputDebug("calcPHashAlgo: second for-loop" + to_string(matrix[12][13]));

    // extract the top 8x8 pixels from the DCT matrix
    int thisIndex = -1;
    std::array<double, 64>   fpexels;
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            thisIndex++;
            fpexels[thisIndex] = matrix[y][x];
        }
    }
    // fnOutputDebug("calcPHashAlgo: third for-loop" + to_string(fpexels[13]));

    INT64 one = 0x0000000000000001;
    INT64 hash = 0x0000000000000000;

    // Calculate hash
    double compareTerm = calcArrayAvgMedian(fpexels, compareMethod);
    // fnOutputDebug("calcPHashAlgo: compareTerm =" + to_string(compareTerm));
    for (int x = 0; x < 64; x++)
    {
        // resultsArray[x] = (fpexels[x] > compareTerm) ? 1 : 0;
        if (fpexels[x] > compareTerm)
           hash |= one;
        one = one << 1;
    }

    // fnOutputDebug("calcPHashAlgo: ended=" + to_string(hash));
    return hash;
}

template <typename T>
inline void SafeRelease(T *&p)
{
    if (NULL != p)
    {
        p->Release();
        p = NULL;
    }
}

INT indexedPixelFmts(WICPixelFormatGUID oPixFmt) {
    INT uPixFmt = 0;
    if (oPixFmt==GUID_WICPixelFormatDontCare)
       uPixFmt = 1;
    else if (oPixFmt==GUID_WICPixelFormat1bppIndexed)
       uPixFmt = 2;
    else if (oPixFmt==GUID_WICPixelFormat2bppIndexed)
       uPixFmt = 3;
    else if (oPixFmt==GUID_WICPixelFormat4bppIndexed)
       uPixFmt = 4;
    else if (oPixFmt==GUID_WICPixelFormat8bppIndexed)
       uPixFmt = 5;
    else if (oPixFmt==GUID_WICPixelFormatBlackWhite)
       uPixFmt = 6;
    else if (oPixFmt==GUID_WICPixelFormat2bppGray)
       uPixFmt = 7;
    else if (oPixFmt==GUID_WICPixelFormat4bppGray)
       uPixFmt = 8;
    else if (oPixFmt==GUID_WICPixelFormat8bppGray)
       uPixFmt = 9;
    else if (oPixFmt==GUID_WICPixelFormat8bppAlpha)
       uPixFmt = 10;
    else if (oPixFmt==GUID_WICPixelFormat16bppBGR555)
       uPixFmt = 11;
    else if (oPixFmt==GUID_WICPixelFormat16bppBGR565)
       uPixFmt = 12;
    else if (oPixFmt==GUID_WICPixelFormat16bppBGRA5551)
       uPixFmt = 13;
    else if (oPixFmt==GUID_WICPixelFormat16bppGray)
       uPixFmt = 14;
    else if (oPixFmt==GUID_WICPixelFormat24bppBGR)
       uPixFmt = 15;
    else if (oPixFmt==GUID_WICPixelFormat24bppRGB)
       uPixFmt = 16;
    else if (oPixFmt==GUID_WICPixelFormat32bppBGR)
       uPixFmt = 17;
    else if (oPixFmt==GUID_WICPixelFormat32bppBGRA)
       uPixFmt = 18;
    else if (oPixFmt==GUID_WICPixelFormat32bppPBGRA)
       uPixFmt = 19;
    else if (oPixFmt==GUID_WICPixelFormat32bppGrayFloat)
       uPixFmt = 20;
    else if (oPixFmt==GUID_WICPixelFormat32bppRGB)
       uPixFmt = 21;
    else if (oPixFmt==GUID_WICPixelFormat32bppRGBA)
       uPixFmt = 22;
    else if (oPixFmt==GUID_WICPixelFormat32bppPRGBA)
       uPixFmt = 23;
    else if (oPixFmt==GUID_WICPixelFormat48bppRGB)
       uPixFmt = 24;
    else if (oPixFmt==GUID_WICPixelFormat48bppBGR)
       uPixFmt = 25;
    else if (oPixFmt==GUID_WICPixelFormat64bppRGB)
       uPixFmt = 26;
    else if (oPixFmt==GUID_WICPixelFormat64bppRGBA)
       uPixFmt = 27;
    else if (oPixFmt==GUID_WICPixelFormat64bppBGRA)
       uPixFmt = 28;
    else if (oPixFmt==GUID_WICPixelFormat64bppPRGBA)
       uPixFmt = 29;
    else if (oPixFmt==GUID_WICPixelFormat64bppPBGRA)
       uPixFmt = 30;
    else if (oPixFmt==GUID_WICPixelFormat16bppGrayFixedPoint)
       uPixFmt = 31;
    else if (oPixFmt==GUID_WICPixelFormat32bppBGR101010)
       uPixFmt = 32;
    else if (oPixFmt==GUID_WICPixelFormat48bppRGBFixedPoint)
       uPixFmt = 33;
    else if (oPixFmt==GUID_WICPixelFormat48bppBGRFixedPoint)
       uPixFmt = 34;
    else if (oPixFmt==GUID_WICPixelFormat96bppRGBFixedPoint)
       uPixFmt = 35;
    else if (oPixFmt==GUID_WICPixelFormat96bppRGBFloat)
       uPixFmt = 36;
    else if (oPixFmt==GUID_WICPixelFormat128bppRGBAFloat)
       uPixFmt = 37;
    else if (oPixFmt==GUID_WICPixelFormat128bppPRGBAFloat)
       uPixFmt = 38;
    else if (oPixFmt==GUID_WICPixelFormat128bppRGBFloat)
       uPixFmt = 39;
    else if (oPixFmt==GUID_WICPixelFormat32bppCMYK)
       uPixFmt = 40;
    else if (oPixFmt==GUID_WICPixelFormat64bppRGBAFixedPoint)
       uPixFmt = 41;
    else if (oPixFmt==GUID_WICPixelFormat64bppBGRAFixedPoint)
       uPixFmt = 42;
    else if (oPixFmt==GUID_WICPixelFormat64bppRGBFixedPoint)
       uPixFmt = 43;
    else if (oPixFmt==GUID_WICPixelFormat128bppRGBAFixedPoint)
       uPixFmt = 44;
    else if (oPixFmt==GUID_WICPixelFormat128bppRGBFixedPoint)
       uPixFmt = 45;
    else if (oPixFmt==GUID_WICPixelFormat64bppRGBAHalf)
       uPixFmt = 46;
    else if (oPixFmt==GUID_WICPixelFormat64bppPRGBAHalf)
       uPixFmt = 47;
    else if (oPixFmt==GUID_WICPixelFormat64bppRGBHalf)
       uPixFmt = 48;
    else if (oPixFmt==GUID_WICPixelFormat48bppRGBHalf)
       uPixFmt = 49;
    else if (oPixFmt==GUID_WICPixelFormat32bppRGBE)
       uPixFmt = 50;
    else if (oPixFmt==GUID_WICPixelFormat16bppGrayHalf)
       uPixFmt = 51;
    else if (oPixFmt==GUID_WICPixelFormat32bppGrayFixedPoint)
       uPixFmt = 52;
    else if (oPixFmt==GUID_WICPixelFormat32bppRGBA1010102)
       uPixFmt = 53;
    else if (oPixFmt==GUID_WICPixelFormat32bppRGBA1010102XR)
       uPixFmt = 54;
    else if (oPixFmt==GUID_WICPixelFormat32bppR10G10B10A2)
       uPixFmt = 55;
    else if (oPixFmt==GUID_WICPixelFormat32bppR10G10B10A2HDR10)
       uPixFmt = 56;
    else if (oPixFmt==GUID_WICPixelFormat64bppCMYK)
       uPixFmt = 57;
    else if (oPixFmt==GUID_WICPixelFormat24bpp3Channels)
       uPixFmt = 58;
    else if (oPixFmt==GUID_WICPixelFormat32bpp4Channels)
       uPixFmt = 59;
    else if (oPixFmt==GUID_WICPixelFormat40bpp5Channels)
       uPixFmt = 60;
    else if (oPixFmt==GUID_WICPixelFormat48bpp6Channels)
       uPixFmt = 61;
    else if (oPixFmt==GUID_WICPixelFormat56bpp7Channels)
       uPixFmt = 62;
    else if (oPixFmt==GUID_WICPixelFormat64bpp8Channels)
       uPixFmt = 63;
    else if (oPixFmt==GUID_WICPixelFormat48bpp3Channels)
       uPixFmt = 64;
    else if (oPixFmt==GUID_WICPixelFormat64bpp4Channels)
       uPixFmt = 65;
    else if (oPixFmt==GUID_WICPixelFormat80bpp5Channels)
       uPixFmt = 66;
    else if (oPixFmt==GUID_WICPixelFormat96bpp6Channels)
       uPixFmt = 67;
    else if (oPixFmt==GUID_WICPixelFormat112bpp7Channels)
       uPixFmt = 68;
    else if (oPixFmt==GUID_WICPixelFormat128bpp8Channels)
       uPixFmt = 69;
    else if (oPixFmt==GUID_WICPixelFormat40bppCMYKAlpha)
       uPixFmt = 70;
    else if (oPixFmt==GUID_WICPixelFormat80bppCMYKAlpha)
       uPixFmt = 71;
    else if (oPixFmt==GUID_WICPixelFormat32bpp3ChannelsAlpha)
       uPixFmt = 72;
    else if (oPixFmt==GUID_WICPixelFormat40bpp4ChannelsAlpha)
       uPixFmt = 73;
    else if (oPixFmt==GUID_WICPixelFormat48bpp5ChannelsAlpha)
       uPixFmt = 74;
    else if (oPixFmt==GUID_WICPixelFormat56bpp6ChannelsAlpha)
       uPixFmt = 75;
    else if (oPixFmt==GUID_WICPixelFormat64bpp7ChannelsAlpha)
       uPixFmt = 76;
    else if (oPixFmt==GUID_WICPixelFormat72bpp8ChannelsAlpha)
       uPixFmt = 77;
    else if (oPixFmt==GUID_WICPixelFormat64bpp3ChannelsAlpha)
       uPixFmt = 78;
    else if (oPixFmt==GUID_WICPixelFormat80bpp4ChannelsAlpha)
       uPixFmt = 79;
    else if (oPixFmt==GUID_WICPixelFormat96bpp5ChannelsAlpha)
       uPixFmt = 80;
    else if (oPixFmt==GUID_WICPixelFormat112bpp6ChannelsAlpha)
       uPixFmt = 81;
    else if (oPixFmt==GUID_WICPixelFormat128bpp7ChannelsAlpha)
       uPixFmt = 82;
    else if (oPixFmt==GUID_WICPixelFormat144bpp8ChannelsAlpha)
       uPixFmt = 83;
    else if (oPixFmt==GUID_WICPixelFormat8bppY)
       uPixFmt = 84;
    else if (oPixFmt==GUID_WICPixelFormat8bppCb)
       uPixFmt = 85;
    else if (oPixFmt==GUID_WICPixelFormat8bppCr)
       uPixFmt = 86;
    else if (oPixFmt==GUID_WICPixelFormat16bppCbCr)
       uPixFmt = 87;
    else if (oPixFmt==GUID_WICPixelFormat16bppYQuantizedDctCoefficients)
       uPixFmt = 88;
    else if (oPixFmt==GUID_WICPixelFormat16bppCbQuantizedDctCoefficients)
       uPixFmt = 89;
    else if (oPixFmt==GUID_WICPixelFormat16bppCrQuantizedDctCoefficients)
       uPixFmt = 90;
    return uPixFmt;
}

INT indexedContainerFmts(GUID containerFmt) {
    INT ucontainerFmt = 0;
    if (containerFmt == GUID_ContainerFormatBmp)
       ucontainerFmt = 1;
    else if (containerFmt == GUID_ContainerFormatPng)
       ucontainerFmt = 2;
    else if (containerFmt == GUID_ContainerFormatIco)
       ucontainerFmt = 3;
    else if (containerFmt == GUID_ContainerFormatJpeg)
       ucontainerFmt = 4;
    else if (containerFmt == GUID_ContainerFormatTiff)
       ucontainerFmt = 5;
    else if (containerFmt == GUID_ContainerFormatGif)
       ucontainerFmt = 6;
    else if (containerFmt == GUID_ContainerFormatWmp)
       ucontainerFmt = 7;
    else if (containerFmt == GUID_ContainerFormatDds)
       ucontainerFmt = 8;
    else if (containerFmt == GUID_ContainerFormatAdng)
       ucontainerFmt = 9;
    else if (containerFmt == GUID_ContainerFormatHeif)
       ucontainerFmt = 10;
    else if (containerFmt == GUID_ContainerFormatWebp)
       ucontainerFmt = 11;
    else if (containerFmt == GUID_ContainerFormatRaw)
       ucontainerFmt = 12;
    return ucontainerFmt;
}

auto adaptImageGivenSize(UINT keepAratio, UINT ScaleAnySize, UINT imgW, UINT imgH, UINT givenW, UINT givenH) {
  std::array<UINT, 3> size;
  size[0] = 0;
  size[1] = 0;
  size[2] = 0;

  if (keepAratio==2)
  {
     size[0] = imgW;
     size[1] = imgH;
     size[2] = 1;
  } else if (keepAratio==1) 
  {
     if (imgW>givenW || imgH>givenH || ScaleAnySize==1)
     {
         double PicRatio = (float)(imgW)/imgH;
         double givenRatio = (float)(givenW)/givenH;
         if (imgW<=givenW && imgH<=givenH)
         {
            size[0] = givenW;
            size[1] = round(size[0] / PicRatio);
            if (size[1]>givenH)
            {
               size[1] = (imgH <= givenH) ? givenH : imgH;
               size[0] = round(size[1] * PicRatio);
            }
         } else if (PicRatio>givenRatio)
         {
            size[0] = givenW;
            size[1] = round(size[0] / PicRatio);
         } else
         {
            size[1] = (imgH >= givenH) ? givenH : imgH;
            size[0] = round(size[1] * PicRatio);
         }
     } else
     {
         size[0] = imgW;
         size[1] = imgH;
         size[2] = 1;
     }
  } else
  {
     size[0] = givenW;
     size[1] = givenH;
  }

  double mpx = (size[0] * size[1])/1000000;
  if (mpx>536.4)
  {
     float g = 536.4/mpx;
     size[0] = floor(size[0] * g);
     size[1] = floor(size[1] * g);
  }

  return size;
}

DLL_API int DLL_CALLCONV WICdestroyPreloadedImage(int id) {
  SafeRelease(pWICclassPixelsBitmapSource);
  pWICclassPixelsBitmapSource = NULL;
  SafeRelease(pWICclassFrameDecoded);
  pWICclassFrameDecoded = NULL;
  SafeRelease(pWICclassDecoder);
  pWICclassDecoder = NULL;
  return id;
}

DLL_API Gdiplus::GpBitmap* DLL_CALLCONV WICgetRectImage(int x, int y, int w, int h, int newW, int newH, int mustClip) {
  Gdiplus::GpBitmap   *myBitmap = NULL;
  IWICBitmapClipper   *pIClipper = NULL;
  IWICBitmapSource    *pBitmapSource = NULL;
  IWICBitmapScaler    *pScaler = NULL;
  IWICBitmapSource    *pFinalBitmapSource = NULL;
  IWICFormatConverter *pConverter = NULL;

  WICRect rcClip = { x, y, w, h };
  HRESULT hr;
  if (mustClip==1)
  {
      hr = m_pIWICFactory->CreateBitmapClipper(&pIClipper);
      if (SUCCEEDED(hr))
         hr = pIClipper->Initialize(pWICclassPixelsBitmapSource, &rcClip);

      fnOutputDebug("clip wic img: " + std::to_string(w) + " / " + std::to_string(h));
      // Retrieve IWICBitmapSource from the frame
      if (SUCCEEDED(hr))
      {
         hr = pIClipper->QueryInterface(IID_IWICBitmapSource, 
                                     reinterpret_cast<void **>(&pBitmapSource));
      }
  } else hr = S_OK;

  if (SUCCEEDED(hr))
     hr = m_pIWICFactory->CreateBitmapScaler(&pScaler);

  if (SUCCEEDED(hr))
  {
     fnOutputDebug("rescale wic img: " + std::to_string(newW) + " / " + std::to_string(newH));
     if (mustClip==1)
        hr = pScaler->Initialize(pBitmapSource, newW, newH, WICBitmapInterpolationModeNearestNeighbor);
     else
        hr = pScaler->Initialize(pWICclassPixelsBitmapSource, newW, newH, WICBitmapInterpolationModeNearestNeighbor);
  }

  if (SUCCEEDED(hr))
  {
      hr = m_pIWICFactory->CreateFormatConverter(&pConverter);
      if (SUCCEEDED(hr))
      {
          hr = pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
          if (SUCCEEDED(hr))
          {
             hr = pConverter->QueryInterface(IID_IWICBitmapSource, reinterpret_cast<void **>(&pFinalBitmapSource));
                              // IID_PPV_ARGS(ppToRenderBitmapSource));
          }
          fnOutputDebug("WIC PIXEL Format converted");
      }
  }

  // Create a DIB from the IWICBitmapSource
  if (SUCCEEDED(hr))
  {
      hr = S_OK;
      UINT width = 0;
      UINT height = 0;

      // Check BitmapSource format
      WICPixelFormatGUID pixelFormat;
      hr = pFinalBitmapSource->GetPixelFormat(&pixelFormat);

      if (SUCCEEDED(hr))
         hr = (pixelFormat == GUID_WICPixelFormat32bppPBGRA) ? S_OK : E_FAIL;

      if (SUCCEEDED(hr))
         hr = pFinalBitmapSource->GetSize(&width, &height); 

      // Size of a scan line represented in bytes: 4 bytes each pixel
      UINT cbStride = 0;
      if (SUCCEEDED(hr))
         hr = UIntMult(width, sizeof(Gdiplus::ARGB), &cbStride);

      // Size of the image, represented in bytes
      UINT cbBufferSize = 0;
      if (SUCCEEDED(hr))
         hr = UIntMult(cbStride, height, &cbBufferSize);

      fnOutputDebug("WIC prepare gdi+ obj");
      if (SUCCEEDED(hr))
      {
          BYTE *m_pbBuffer = NULL;  // the GDI+ bitmap buffer
          m_pbBuffer = new (std::nothrow) BYTE[cbBufferSize];
          hr = (m_pbBuffer!=nullptr) ? S_OK : E_FAIL;
          if (SUCCEEDED(hr))
          {
              fnOutputDebug("WIC gdi+ obj buffer");
              // WICRect rc = { 0, 0, width, height };
              // Extract the image into the GDI+ Bitmap
              // fnOutputDebug("WIC pre-copy pixels");
              hr = pFinalBitmapSource->CopyPixels(NULL, cbStride, cbBufferSize, m_pbBuffer);
              // fnOutputDebug("WIC after copy pixels");
              if (SUCCEEDED(hr))
              {
                 fnOutputDebug("WIC gdi+ obj copy pixels into buffer");
                 Gdiplus::DllExports::GdipCreateBitmapFromScan0(width, height, cbStride, PixelFormat32bppPARGB, NULL, &myBitmap);
                 Gdiplus::Rect rectu(0, 0, width, height);
                 Gdiplus::BitmapData bitmapDatu;
                 bitmapDatu.Width = width;
                 bitmapDatu.Height = height;
                 bitmapDatu.Stride = cbStride;
                 bitmapDatu.PixelFormat = PixelFormat32bppPARGB;
                 bitmapDatu.Scan0 = m_pbBuffer;

                 Gdiplus::DllExports::GdipBitmapLockBits(myBitmap, &rectu, 6, PixelFormat32bppPARGB, &bitmapDatu);
                 Gdiplus::DllExports::GdipBitmapUnlockBits(myBitmap, &bitmapDatu);
                 hr = myBitmap ? S_OK : E_FAIL;
                 if (SUCCEEDED(hr))
                    fnOutputDebug("WIC gdi+ obj created");
              }
              delete[] m_pbBuffer;
          }
          m_pbBuffer = NULL; 
      }
  }

  SafeRelease(pIClipper);
  SafeRelease(pConverter);
  SafeRelease(pScaler);
  SafeRelease(pBitmapSource);
  SafeRelease(pFinalBitmapSource);
  return myBitmap;
}

DLL_API BYTE* DLL_CALLCONV WICgetLargeBufferImage(int okay, int bitsDepth, UINT64 cbStride, UINT64 cbBufferSize, int sliceHeight) {
  okay = 0;
  HRESULT hr;
  IWICFormatConverter *pConverter = NULL;
  IWICBitmapSource *pFinalBitmapSource = NULL;

  UINT width = 0;
  UINT height = 0;
  // UINT cbStride = 0;
  // double cbBufferSize = 0;
  hr = pWICclassPixelsBitmapSource->GetSize(&width, &height); 
  if (SUCCEEDED(hr))
     hr = m_pIWICFactory->CreateFormatConverter(&pConverter);

  if (SUCCEEDED(hr))
  {
     if (bitsDepth==32)
        hr = pConverter->Initialize(pWICclassPixelsBitmapSource, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
     else
        hr = pConverter->Initialize(pWICclassPixelsBitmapSource, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
  }

  if (SUCCEEDED(hr))
     hr = pConverter->QueryInterface(IID_IWICBitmapSource, reinterpret_cast<void **>(&pFinalBitmapSource));

  BYTE *m_pbBuffer = NULL;
  m_pbBuffer = new (std::nothrow) BYTE[cbBufferSize];
  hr = (m_pbBuffer!=nullptr) ? S_OK : E_FAIL;

  int y = 0;
  int indexu = 0;
  UINT64 buffOffset = 0;
  if (SUCCEEDED(hr))
  {
      fnOutputDebug(std::to_string(cbStride) + " WIC buffer created: " + std::to_string(cbBufferSize) + "; h=" + std::to_string(height) + "; w=" + std::to_string(width));
      while (y<height)
      {
          if (indexu>0)
             y += sliceHeight;
          if (y>=height)
             break;

          int h = (y + sliceHeight>height) ? height - y : sliceHeight;
          WICRect rc = { 0, y, width, h };
          UINT tmpBufferSize = cbStride * h;
          // fnOutputDebug(std::to_string(indexu) + "# y=" + std::to_string(y) + "; h=" + std::to_string(h));
          // fnOutputDebug("tmp buffer prepped:" + std::to_string(tmpBufferSize));
          HRESULT hr = pFinalBitmapSource->CopyPixels(&rc, cbStride, tmpBufferSize, m_pbBuffer + buffOffset);
          if (SUCCEEDED(hr))
          {
             // fnOutputDebug("tmp buffer YAY");
             buffOffset += tmpBufferSize;
          }
          indexu++;
          // delete[] tmp_Buffer;
      }

      if (SUCCEEDED(hr))
      {
          okay = 1;
          fnOutputDebug("WIC copy pixels to buffer: yay");
      } else
      {
         delete[] m_pbBuffer;
         m_pbBuffer = NULL;
      }
  }

  SafeRelease(pFinalBitmapSource);
  SafeRelease(pConverter);
  return m_pbBuffer;
}

DLL_API BYTE* DLL_CALLCONV WICgetBufferImage(int okay, int bitsDepth, UINT64 cbStride, UINT64 cbBufferSize) {
  okay = 0;
  HRESULT hr;
  IWICFormatConverter *pConverter = NULL;
  IWICBitmapSource *pFinalBitmapSource = NULL;

  UINT width = 0;
  UINT height = 0;
  hr = pWICclassPixelsBitmapSource->GetSize(&width, &height); 
  if (SUCCEEDED(hr))
     hr = m_pIWICFactory->CreateFormatConverter(&pConverter);

  if (SUCCEEDED(hr))
  {
     if (bitsDepth==32)
        hr = pConverter->Initialize(pWICclassPixelsBitmapSource, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
     else
        hr = pConverter->Initialize(pWICclassPixelsBitmapSource, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
  }

  if (SUCCEEDED(hr))
     hr = pConverter->QueryInterface(IID_IWICBitmapSource, reinterpret_cast<void **>(&pFinalBitmapSource));

  BYTE *m_pbBuffer = NULL;
  m_pbBuffer = new (std::nothrow) BYTE[cbBufferSize];
  hr = (m_pbBuffer!=nullptr) ? S_OK : E_FAIL;
  if (SUCCEEDED(hr))
  {
      fnOutputDebug("WIC buffer created: " + std::to_string(cbBufferSize));
      hr = pFinalBitmapSource->CopyPixels(NULL, cbStride, cbBufferSize, m_pbBuffer);
      if (SUCCEEDED(hr))
      {
          okay = 1;
          fnOutputDebug("WIC copy pixels to buffer: yay");
      } else
      {
         delete[] m_pbBuffer;
         m_pbBuffer = NULL;
      }
  }

  SafeRelease(pFinalBitmapSource);
  SafeRelease(pConverter);
  return m_pbBuffer;
}

DLL_API int DLL_CALLCONV WICpreLoadImage(const wchar_t *szFileName, int givenFrame, UINT *resultsArray) {
  // IWICBitmapDecoder *pWICclassDecoder = NULL;
  HRESULT hr = S_OK;
  try {
      hr = m_pIWICFactory->CreateDecoderFromFilename(szFileName,NULL,GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pWICclassDecoder);
  } catch (const char* message) {
      std::stringstream ss;
      ss << "qpv: WIC decoder error on file " << szFileName;
      ss << " WIC error " << message;
      OutputDebugStringA(ss.str().data());
      return 0;
  }

  if (SUCCEEDED(hr))
  {
      // IWICBitmapFrameDecode *pWICclassFrameDecoded = NULL;
      UINT tFrames = 0;
      hr = pWICclassDecoder->GetFrameCount(&tFrames);
      if (givenFrame > tFrames - 1)
         givenFrame = tFrames - 1;

      resultsArray[2] = tFrames;
      resultsArray[6] = givenFrame;
      hr = pWICclassDecoder->GetFrame(givenFrame, &pWICclassFrameDecoded);
      // std::stringstream ss;
      // ss << "qpv: decoder tFrames " << tFrames;
      // ss << " givenFrame " << givenFrame;
      // OutputDebugStringA(ss.str().data());
  } else
  {
      std::stringstream ss;
      ss << "qpv: WIC decoder error on file " << szFileName;
      OutputDebugStringA(ss.str().data());
      return 0;
  };

  // Retrieve IWICBitmapSource from the frame
  if (SUCCEEDED(hr))
  {
      // IWICBitmapSource   *pWICclassPixelsBitmapSource = NULL;
      GUID containerFmt;
      hr = pWICclassDecoder->GetContainerFormat(&containerFmt);
      UINT ucontainerFmt = indexedContainerFmts(containerFmt);
      resultsArray[5] = ucontainerFmt;

      hr = pWICclassFrameDecoded->QueryInterface(IID_IWICBitmapSource, 
                                  reinterpret_cast<void **>(&pWICclassPixelsBitmapSource));
  }

  if (SUCCEEDED(hr))
  {
      // std::stringstream ss;
      // ss << "qpv: collect image infos ";
      // OutputDebugStringA(ss.str().data());
      hr = S_OK;
      UINT owidth = 0;
      UINT oheight = 0;
      hr = pWICclassPixelsBitmapSource->GetSize(&owidth, &oheight); 
      resultsArray[0] = owidth;
      resultsArray[1] = oheight;

      double dpix = 0;
      double dpiy = 0;
      hr = pWICclassPixelsBitmapSource->GetResolution(&dpix, &dpiy); 
      resultsArray[4] = round((dpix + dpiy)/2);

      WICPixelFormatGUID opixelFormat;
      hr = pWICclassPixelsBitmapSource->GetPixelFormat(&opixelFormat);
      UINT uPixFmt = indexedPixelFmts(opixelFormat);
      resultsArray[3] = uPixFmt;
      return 1;
  } else return 0;
}

DLL_API Gdiplus::GpBitmap* DLL_CALLCONV LoadWICimage(int threadIDu, int noBPPconv, int givenQuality, UINT givenW, UINT givenH, UINT keepAratio, UINT ScaleAnySize, UINT givenFrame, int doFlip, int doGrayScale, const wchar_t *szFileName, UINT *resultsArray) {
    Gdiplus::GpBitmap  *myBitmap = NULL;

    WICBitmapInterpolationMode wicScaleQuality;
    if (givenQuality==5)
       wicScaleQuality = WICBitmapInterpolationModeNearestNeighbor;
    else if (givenQuality==2)
       wicScaleQuality = WICBitmapInterpolationModeLinear;
    else if (givenQuality==3)
       wicScaleQuality = WICBitmapInterpolationModeCubic;
    else if (givenQuality==7)
       wicScaleQuality = WICBitmapInterpolationModeHighQualityCubic;
    else
       wicScaleQuality = WICBitmapInterpolationModeFant;

    // std::stringstream ss;
    if (szFileName)
    {
        IWICBitmapSource   *m_pOriginalBitmapSource = NULL;
        IWICBitmapSource   *pToRenderBitmapSource = NULL;
        // IWICBitmapSource   *gToRenderBitmapSource = NULL;
        HRESULT hr = S_OK;
        HRESULT hr2 = S_OK;
        HRESULT hrFlip = S_SERDDR;
        HRESULT hrGray = S_SERDDR;
        IWICBitmapFlipRotator* pIFlipRotator = NULL;
        IWICFormatConverter* pConverterGray = NULL;

        // Step 0: Create WIC factory [ see initWICnow() ]
        // Step 1: Create a decoder
        // Decode the source image to IWICBitmapSource
        IWICBitmapDecoder *pDecoder = NULL;
        try {
            hr = m_pIWICFactory->CreateDecoderFromFilename(szFileName,                   // Image to be decoded
                                                        NULL,                            // Do not prefer a particular vendor
                                                        GENERIC_READ,                    // Desired read access to the file
                                                        WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
                                                        &pDecoder);                      // Pointer to the decoder
        } catch (const char* message) {
            std::stringstream ss;
            ss << "qpv: threadu - " << threadIDu << " WIC decoder error on file " << szFileName;
            ss << " WIC error " << message;
            OutputDebugStringA(ss.str().data());
            return myBitmap;
        }

        // Step 2: Retrieve the first frame of the image from the decoder
        IWICBitmapFrameDecode *pFrame = NULL;
        if (SUCCEEDED(hr))
        {
            UINT tFrames = 0;
            hr2 = pDecoder->GetFrameCount(&tFrames);
            resultsArray[2] = tFrames;
            if (givenFrame > tFrames - 1)
               givenFrame = tFrames - 1;

            hr = pDecoder->GetFrame(givenFrame, &pFrame);
            // std::stringstream ss;
            // ss << "qpv: threadu - " << threadIDu << " decoder tFrames " << tFrames;
            // ss << " givenFrame " << givenFrame;
            // OutputDebugStringA(ss.str().data());
        } else
        {
            std::stringstream ss;
            ss << "qpv: threadu - " << threadIDu << " WIC decoder error on file " << szFileName;
            OutputDebugStringA(ss.str().data());
            return myBitmap;
        };

        // Retrieve IWICBitmapSource from the frame
        // m_pOriginalBitmapSource contains the original bitmap and acts as an intermediate
        if (SUCCEEDED(hr))
        {
            // SafeRelease(m_pOriginalBitmapSource);
            GUID containerFmt;
            hr2 = pDecoder->GetContainerFormat(&containerFmt);
            UINT ucontainerFmt = indexedContainerFmts(containerFmt);
            resultsArray[5] = ucontainerFmt;

            hr = pFrame->QueryInterface(IID_IWICBitmapSource, 
                         reinterpret_cast<void **>(&m_pOriginalBitmapSource));

            // std::stringstream ss;
            // ss << "qpv: threadu - " << threadIDu << " get WIC frame image " << hr;
            // OutputDebugStringA(ss.str().data());
        }

        // Step 3: Scale the original IWICBitmapSource to the given size
        // and convert the pixel format
        // IWICBitmapSource *pToRenderBitmapSource = NULL;

        if (SUCCEEDED(hr))
        {
            // std::stringstream ss;
            // ss << "qpv: threadu - " << threadIDu << " collect image infos ";
            // OutputDebugStringA(ss.str().data());
            hr = S_OK;
            // IWICBitmapSource** ppToRenderBitmapSource = NULL;
            // *ppToRenderBitmapSource = NULL;

            UINT owidth = 0;
            UINT oheight = 0;
            hr2 = m_pOriginalBitmapSource->GetSize(&owidth, &oheight); 
            resultsArray[0] = owidth;
            resultsArray[1] = oheight;

            double dpix = 0;
            double dpiy = 0;
            hr2 = m_pOriginalBitmapSource->GetResolution(&dpix, &dpiy); 
            resultsArray[4] = round((dpix + dpiy)/2);

            WICPixelFormatGUID opixelFormat;
            hr2 = m_pOriginalBitmapSource->GetPixelFormat(&opixelFormat);
            UINT uPixFmt = indexedPixelFmts(opixelFormat);

            resultsArray[3] = uPixFmt;
            if (noBPPconv==1)
            {
               resultsArray[6] = 1;
               SafeRelease(pToRenderBitmapSource);
               SafeRelease(pDecoder);
               SafeRelease(pFrame);
               SafeRelease(m_pOriginalBitmapSource);
               return myBitmap;
            }

            if (SUCCEEDED(hr) && doGrayScale==1)
            {
               hrGray = m_pIWICFactory->CreateFormatConverter(&pConverterGray);
               if (SUCCEEDED(hr))
               {
                   hrGray = pConverterGray->Initialize(m_pOriginalBitmapSource,
                                     GUID_WICPixelFormat8bppGray,
                                     WICBitmapDitherTypeNone, NULL, 0.f,
                                     WICBitmapPaletteTypeCustom);
   
                   // if (SUCCEEDED(hrGray))
                   // {
                   //    hrGray = pConverterGray->QueryInterface(IID_IWICBitmapSource, 
                   //                  reinterpret_cast<void **>(&gToRenderBitmapSource));
                   // }
               }
            }

            if (SUCCEEDED(hr))
            {
                if (doFlip==4)
                {
                   hrFlip = m_pIWICFactory->CreateBitmapFlipRotator(&pIFlipRotator);
                   if (SUCCEEDED(hrFlip))
                   {
                      if (SUCCEEDED(hrGray))
                         pIFlipRotator->Initialize(pConverterGray, WICBitmapTransformFlipHorizontal);
                      else
                         pIFlipRotator->Initialize(m_pOriginalBitmapSource, WICBitmapTransformFlipHorizontal);
                   }
                }

                // Create a BitmapScaler
                IWICBitmapScaler* pScaler = NULL;
                hr = m_pIWICFactory->CreateBitmapScaler(&pScaler);
                // std::stringstream ss;
                // ss << "qpv: threadu - " << threadIDu << " scale image " << hr;
                // OutputDebugStringA(ss.str().data());

                // Initialize the bitmap scaler from the original bitmap map bits
                if (SUCCEEDED(hr))
                {
                    auto nSize = adaptImageGivenSize(keepAratio, ScaleAnySize, owidth, oheight, givenW, givenH);
                    if (nSize[2]==1)
                       wicScaleQuality = WICBitmapInterpolationModeNearestNeighbor;

                    if (SUCCEEDED(hrFlip))
                    {
                       hr = pScaler->Initialize(pIFlipRotator, nSize[0], nSize[1], wicScaleQuality);
                    } else
                    {
                       if (SUCCEEDED(hrGray))
                          hr = pScaler->Initialize(pConverterGray, nSize[0], nSize[1], wicScaleQuality);
                       else
                          hr = pScaler->Initialize(m_pOriginalBitmapSource, nSize[0], nSize[1], wicScaleQuality);
                    }

                    // std::stringstream ss;
                    // ss << "qpv: threadu - " << threadIDu << " scaled image to W/H " << nSize[0] << "," << nSize[1];
                    // ss << " - quality " << wicScaleQuality;
                    // ss << " - oW/H " << owidth << "," << oheight;
                    // ss << " - gW/H " << givenW << "," << givenH;
                    // OutputDebugStringA(ss.str().data());
                }

                // Format convert the bitmap into 32bppBGR, a convenient 
                // pixel format for GDI+ rendering 
                if (SUCCEEDED(hr))
                {
                    IWICFormatConverter* pConverter = NULL;
                    hr = m_pIWICFactory->CreateFormatConverter(&pConverter);
                    // std::stringstream ss;
                    // ss << "qpv: threadu - " << threadIDu << " do convert pix format h=" << oheight;
                    // OutputDebugStringA(ss.str().data());

                    // Format convert to 32bppBGR 
                    if (SUCCEEDED(hr))
                    {
                        hr = pConverter->Initialize(pScaler,            // Input bitmap to convert
                                          GUID_WICPixelFormat32bppPBGRA,// Destination pixel format
                                          WICBitmapDitherTypeNone,      // Specified dither patterm
                                          NULL,                         // Specify a particular palette 
                                          0.f,                          // Alpha threshold
                                          WICBitmapPaletteTypeCustom);  // Palette translation type

                        // std::stringstream ss;
                        // ss << "qpv: threadu - " << threadIDu << " converted pix format h=" << oheight;
                        // OutputDebugStringA(ss.str().data());
                        // Store the converted bitmap as pToRenderBitmapSource 
                        if (SUCCEEDED(hr))
                        {
                           hr = pConverter->QueryInterface(IID_IWICBitmapSource, 
                                            reinterpret_cast<void **>(&pToRenderBitmapSource));
                                            // IID_PPV_ARGS(ppToRenderBitmapSource));
                           // std::stringstream ss;
                           // ss << "qpv: threadu - " << threadIDu << " get bitmap converted format w=" << owidth;
                           // OutputDebugStringA(ss.str().data());
                        };
                    };

                    SafeRelease(pConverter);
                };

                SafeRelease(pScaler);
            };

        };

        // Step 4: Create a DIB from the converted IWICBitmapSource
        if (SUCCEEDED(hr))
        {
            HRESULT hr = S_OK;
            UINT width = 0;
            UINT height = 0;

            // Check BitmapSource format
            WICPixelFormatGUID pixelFormat;
            hr = pToRenderBitmapSource->GetPixelFormat(&pixelFormat);

            if (SUCCEEDED(hr))
               hr = (pixelFormat == GUID_WICPixelFormat32bppPBGRA) ? S_OK : E_FAIL;

            if (SUCCEEDED(hr))
               hr = pToRenderBitmapSource->GetSize(&width, &height); 

            // Size of a scan line represented in bytes: 4 bytes each pixel
            UINT cbStride = 0;
            if (SUCCEEDED(hr))
               hr = UIntMult(width, sizeof(Gdiplus::ARGB), &cbStride);

            // Size of the image, represented in bytes
            UINT cbBufferSize = 0;
            if (SUCCEEDED(hr))
               hr = UIntMult(cbStride, height, &cbBufferSize);

            // std::stringstream ss;
            // ss << "qpv: threadu - " << threadIDu << " convert to dib format Stride=" << cbStride;
            // OutputDebugStringA(ss.str().data());
            if (SUCCEEDED(hr))
            {
                BYTE *m_pbBuffer = NULL;  // the GDI+ bitmap buffer
                m_pbBuffer = new (std::nothrow) BYTE[cbBufferSize];
                hr = (m_pbBuffer!=nullptr) ? S_OK : E_FAIL;

                // std::stringstream ss;
                // ss << "qpv: threadu - " << threadIDu << " prepared dib format buffer ";
                // OutputDebugStringA(ss.str().data());
                if (SUCCEEDED(hr))
                {
                    // WICRect rc = { 0, 0, width, height };
                    // Extract the image into the GDI+ Bitmap
                    hr = pToRenderBitmapSource->CopyPixels(NULL, cbStride, cbBufferSize, m_pbBuffer);

                    // std::stringstream ss;
                    // ss << "qpv: threadu - " << threadIDu << " convert to dib format copied pixels " << hr;
                    // OutputDebugStringA(ss.str().data());
                    if (SUCCEEDED(hr))
                    {
                       Gdiplus::DllExports::GdipCreateBitmapFromScan0(width, height, cbStride, PixelFormat32bppPARGB, NULL, &myBitmap);
                       // std::stringstream ss;
                       // ss << "qpv: threadu - " << threadIDu << " created gdip image " << hr;
                       // OutputDebugStringA(ss.str().data());
                       Gdiplus::Rect rectu(0, 0, width, height);
                       Gdiplus::BitmapData bitmapDatu;
                       bitmapDatu.Width = width;
                       bitmapDatu.Height = height;
                       bitmapDatu.Stride = cbStride;
                       bitmapDatu.PixelFormat = PixelFormat32bppPARGB;
                       bitmapDatu.Scan0 = m_pbBuffer;

                       Gdiplus::DllExports::GdipBitmapLockBits(myBitmap, &rectu, 6, PixelFormat32bppPARGB, &bitmapDatu);
                       Gdiplus::DllExports::GdipBitmapUnlockBits(myBitmap, &bitmapDatu);
                       hr = myBitmap ? S_OK : E_FAIL;

                       // std::stringstream sb;
                       // sb << "qpv: threadu - " << threadIDu << " filled gdip image " << hr;
                       // OutputDebugStringA(sb.str().data());
                    };
                };
                delete[] m_pbBuffer;
                m_pbBuffer = NULL; 
            };
        };

        // std::stringstream ss;
        // ss << "qpv: threadu - " << threadIDu << " do safe releases";
        // OutputDebugStringA(ss.str().data());
        SafeRelease(pToRenderBitmapSource);
        // SafeRelease(gToRenderBitmapSource);
        SafeRelease(pDecoder);
        SafeRelease(pConverterGray);
        SafeRelease(pIFlipRotator);
        SafeRelease(pFrame);
        SafeRelease(m_pOriginalBitmapSource);
        // SafeRelease(m_pIWICFactory);
        // CoUninitialize();
    };

/*
    std::wstring string_to_convert(szFileName);

    //setup converter
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

    //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
    std::string converted_str = converter.to_bytes( string_to_convert );

    // std::stringstream ss;
    ss << "qpv: threadu - " << threadIDu << " bmp file " << converted_str;
    OutputDebugStringA(ss.str().data());
*/
    return myBitmap;
}

int myRound(double x) {
    return (x<0) ? (int)(x-0.5) : (int)(x+0.5);
}

STATUS InsertJPEGFile2PDF(const char *fileName, int fileSize, PJPEG2PDF pdfId) {
  FILE *fp;
  unsigned char *jpegBuf;
  int readInSize; 
  unsigned short jpegImgW, jpegImgH;
  STATUS r = IDOK;

  jpegBuf = (unsigned char *)malloc(fileSize);
  fp = fopen(fileName, "rb");
  readInSize = (int)fread(jpegBuf, sizeof(UINT8), fileSize, fp);
  fclose(fp);

  if (readInSize != fileSize) 
     fnOutputDebug("file size in bytes mismatched: " + std::to_string(readInSize) + " / " + std::to_string(fileSize));

  // Add JPEG File into PDF
  if (1 == get_jpeg_size(jpegBuf, readInSize, &jpegImgW, &jpegImgH))
  {
     std::string s = fileName;
     r = Jpeg2PDF_AddJpeg(pdfId, jpegImgW, jpegImgH, readInSize, jpegBuf, 1);
     fnOutputDebug("Image dimensions: " + std::to_string(jpegImgW) + " x " + std::to_string(jpegImgH) + " | " + s);
  } else
  {
     std::string s = fileName;
     fnOutputDebug("failed to obtain image dimensions from file: " + s);
     r = ERROR;
  }

  free(jpegBuf);
  return r;
}

DLL_API int DLL_CALLCONV CreatePDFfile(const char* tempDir, const char* destinationPDFfile, const char* scriptDir, UINT *fListArray, int arraySize, float pageW, float pageH, int dpi) {
// based on https://www.codeproject.com/Articles/29879/Simplest-PDF-Generating-API-for-JPEG-Image-Content

  // Initializd the PDF Object with Page Size Information
  fnOutputDebug("function CreatePDFfile called" + std::to_string(pageW) + " x " + std::to_string(pageH) );
  PJPEG2PDF pdfId;
  pdfId = Jpeg2PDF_BeginDocument(pageW, pageH, dpi);
  if (pdfId < 0) 
     return -1;
 
  UINT32 pdfSize, pdfFinalSize;
  UINT8  *pdfBuf;

  int dirErr = 0;
  if (_chdir(tempDir))
  {
      switch (errno)
      {
        case ENOENT:
           dirErr = -2;
           break;
        case EINVAL:
           dirErr = -3;
           break;
        default:
           dirErr = -4;
      }
      return dirErr;
  }

  // Process the jpeg files
  fnOutputDebug("about to load images pointed by fListArray.size=" + std::to_string(arraySize));
  struct _finddata_t jpeg_file;
  long hFile;
  int somePagesError = 0;
  for (int i = 0; i < arraySize; ++i)
  {
      std::string s = std::to_string(fListArray[i]) + ".jpg";
      // const char * c = str.c_str();
      // fnOutputDebug("looping array " + std::to_string(i) + " file=" + s);
      if ( (hFile = _findfirst(s.c_str(), &jpeg_file )) == -1L )
         continue;
 
      // fnOutputDebug("found file: " + s);
      STATUS z = InsertJPEGFile2PDF(jpeg_file.name, jpeg_file.size, pdfId);
      if (z==ERROR)
      {
         fnOutputDebug("Failed to add image to PDF: " + s);
         somePagesError++;
      }
      _findclose( hFile );
  }

  // Finalize the PDF and get the PDF Size
  fnOutputDebug("Finalize the PDF and get the PDF Size");
  pdfSize = Jpeg2PDF_EndDocument(pdfId);
  // Prepare the PDF Data Buffer based on the PDF Size
  pdfBuf = (UINT8 * )malloc(pdfSize);

  // Get the PDF into the Data Buffer and do the cleanup
  fnOutputDebug("PDF size = " + std::to_string(pdfSize) + "; next function Jpeg2PDF_GetFinalDocumentAndCleanup()");
  // Output the PDF Data Buffer to file
  STATUS g = Jpeg2PDF_GetFinalDocumentAndCleanup(pdfId, pdfBuf, &pdfFinalSize, pdfSize);
  if (g=IDOK)
  {
     fnOutputDebug("writing PDF: final size =" + std::to_string(pdfFinalSize));
     FILE *fp = fopen(destinationPDFfile, "wb");
     if (fp!=NULL)
     {
        fwrite(pdfBuf, sizeof(UINT8), pdfFinalSize, fp);
        fclose(fp);
     } else 
     {
        fnOutputDebug("Failed to create PDF file");
        dirErr = -6;
     }
  } else 
  {
     fnOutputDebug("Failed to PDF GetFinalDocument");
     dirErr = -7;
  }

  _chdir(scriptDir);
  free(pdfBuf);
  if (dirErr == 0 && somePagesError != 0)
     dirErr = somePagesError;

  return dirErr;
}

Gdiplus::GpBitmap* CreateGdipBitmapFromCImg(CImg<float> & img, int width, int height) {
    // fnOutputDebug("CreateGdipBitmapFromCImg called, yay");
    // Size of a scan line represented in bytes: 4 bytes each pixel
    UINT cbStride = 0;
    UIntMult(width, sizeof(Gdiplus::ARGB), &cbStride);

    // Size of the image, represented in bytes
    UINT cbBufferSize = 0;
    UIntMult(cbStride, height, &cbBufferSize);

    Gdiplus::GpBitmap  *myBitmap = NULL;
    BYTE *m_pbBuffer = NULL;  // the GDI+ bitmap buffer
    m_pbBuffer = new (std::nothrow) BYTE[cbBufferSize];
    if (m_pbBuffer==nullptr)
       return myBitmap;

    // fnOutputDebug("gdip bmp created, yay");
    Gdiplus::DllExports::GdipCreateBitmapFromScan0(width, height, cbStride, PixelFormat32bppARGB, NULL, &myBitmap);
    Gdiplus::Rect rectu(0, 0, width, height);
    Gdiplus::BitmapData bitmapDatu;
    bitmapDatu.Width = width;
    bitmapDatu.Height = height;
    bitmapDatu.Stride = cbStride;
    bitmapDatu.PixelFormat = PixelFormat32bppARGB;
    bitmapDatu.Scan0 = m_pbBuffer;
    const int nPlanes = 4; // NOTE we assume alpha plane is the 4th plane.
 
    Gdiplus::Status s = Gdiplus::DllExports::GdipBitmapLockBits(myBitmap, &rectu, 6, PixelFormat32bppARGB, &bitmapDatu);
    // Step through cimg and bitmap, copy values to bitmap.
    if (s == Gdiplus::Ok)
    {
        // fnOutputDebug("gdip bmp locked, yay");
        // fnOutputDebug("init vars for conversion; Stride=" + std::to_string(dLineDest));
        BYTE *pStartDest = (BYTE *) bitmapDatu.Scan0;
        UINT dPixelDest = nPlanes;             // pixel step in destination
        UINT dLineDest = bitmapDatu.Stride;    // line step in destination
        #pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < height; y++)
        {
            // loop through lines
            BYTE *pLineDest  = pStartDest + dLineDest*y;
            BYTE *pPixelDest = pLineDest;
            for (int x = 0; x < width; x++)
            {
                // loop through pixels on line
                // T & operator() (const unsigned int x, const unsigned int y=0, const unsigned int z=0, const unsigned int v=0) const
                // Fast access to pixel value for reading or writing.

                float    redCompF = img(x,y,0,0);
                if (redCompF < 0.0f)
                    redCompF = 0.0f;
                else if (redCompF > 255.0f)
                    redCompF = 255.0f;

                float    greenCompF = img(x,y,0,1);
                if (greenCompF < 0.0f)
                    greenCompF = 0.0f;
                else if (greenCompF > 255.0f)
                    greenCompF = 255.0f;

                float    blueCompF = img(x,y,0,2);
                if (blueCompF < 0.0f)
                    blueCompF = 0.0f;
                else if (blueCompF > 255.0f)
                    blueCompF = 255.0f;

                float    alphaCompF = img(x,y,0,3);
                if (alphaCompF < 0.0f)
                    alphaCompF = 0.0f;
                else if (alphaCompF > 255.0f)
                    alphaCompF = 255.0f;

                BYTE    redComp = BYTE(redCompF + 0.4999f);
                BYTE    greenComp = BYTE(greenCompF + 0.4999f);
                BYTE    blueComp = BYTE(blueCompF + 0.4999f);
                BYTE    alphaComp = BYTE(alphaCompF + 0.4999f);
                *(pPixelDest) = blueComp;
                *(pPixelDest+1) = greenComp;
                *(pPixelDest+2) = redComp;
                *(pPixelDest+3) = alphaComp;
                pPixelDest += dPixelDest;
            }
            // pLineDest += dLineDest;
        }
        // fnOutputDebug("for loops done");
        Gdiplus::DllExports::GdipBitmapUnlockBits(myBitmap, &bitmapDatu);
        // fnOutputDebug("gdip bmp unlocked");
    }
    delete[] m_pbBuffer;
    m_pbBuffer = NULL; 

    return myBitmap;
}

void FillGdipLockedBitmapDataFromCImg(unsigned char *imageData, CImg<unsigned char> &img, int width, int height, int Stride, int bpp, unsigned char *maskBitmap, int mStride) {
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (mStride>-1)
            {
               if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
                  continue;
            }

            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            imageData[o]     = img(x,y,0,0); // b
            imageData[o + 1] = img(x,y,0,1); // g
            imageData[o + 2] = img(x,y,0,2); // r
            if (bpp==32)
               imageData[o + 3] = img(x,y,0,3); // a
        }
    }
}

void FillGdipLockedBitmapDataFromCImg(unsigned char *imageData, CImg<unsigned char> &img, int width, int height, int Stride, int bpp) {
     FillGdipLockedBitmapDataFromCImg(imageData, img, width, height, Stride, bpp, NULL, -1);
}

int FillCImgFromBitmap(cimg_library::CImg<float> & img, Gdiplus::GpBitmap *myBitmap, int width, int height) {

    // fnOutputDebug("FillCImgFromBitmap called, yay");
    // Size of a scan line represented in bytes: 4 bytes each pixel
    UINT cbStride = 0;
    UIntMult(width, sizeof(Gdiplus::ARGB), &cbStride);

    // Size of the image, represented in bytes
    UINT cbBufferSize = 0;
    UIntMult(cbStride, height, &cbBufferSize);

    Gdiplus::Rect rectu(0, 0, width, height);
    Gdiplus::BitmapData bitmapDatu;
    Gdiplus::Status s = Gdiplus::DllExports::GdipBitmapLockBits(myBitmap, &rectu, 1, PixelFormat32bppARGB, &bitmapDatu);
    // fnOutputDebug("bits locked: FillCImgFromBitmap()");
    if (s == Gdiplus::Ok)
    {
        // fnOutputDebug("for loops begin: FillCImgFromBitmap(); w=" + std::to_string(nPixels) + "; h=" + std::to_string(nLines) );
        // fnOutputDebug("moar; Stride=" + std::to_string(dLineSrc) + "/scan0=" + std::to_string(*pStartSrc));
        BYTE *pStartSrc = (BYTE *) bitmapDatu.Scan0;
        UINT dPixelSrc = 4;          // pixel step in source ; nPlanes
        UINT dLineSrc = cbStride;    // line step in source
        #pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < height; y++)
        {
            // loop through lines
            BYTE *pLineSrc = pStartSrc + dLineSrc*y;
            BYTE *pPixelSrc = pLineSrc;
            // fnOutputDebug("Y loop: " + std::to_string(y) + "//Stride=" + std::to_string(dLineSrc));
            for (int x = 0; x < width; x++)
            {
                // loop through pixels on line
                BYTE alphaComp = *(pPixelSrc+3);
                BYTE redComp = *(pPixelSrc+2);
                BYTE greenComp = *(pPixelSrc+1);
                BYTE blueComp = *(pPixelSrc+0);
                img(x,y,0,0) = float(redComp);
                img(x,y,0,1) = float(greenComp);
                img(x,y,0,2) = float(blueComp);
                img(x,y,0,3) = float(alphaComp);
                pPixelSrc += dPixelSrc;
                // fnOutputDebug("x loop B: " + std::to_string(x));
            }
            // pLineSrc += dLineSrc;
        }
        // fnOutputDebug("for loops done: FillCImgFromBitmap()");
        Gdiplus::DllExports::GdipBitmapUnlockBits(myBitmap, &bitmapDatu);
        // fnOutputDebug("gdip bmp unlocked: FillCImgFromBitmap()");
        return 1;
    } else return 0;
}

int FillCImgFromLockedBitmapData(cimg_library::CImg<unsigned char> & img, unsigned char *myBitmap, int width, int height, int Stride, int bpp, int zx1, int zy1, int zx2, int zy2, int invertArea) {
    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; y++)
    {
        if (invertArea==1)
        {
           if (inRange(zy1, zy2, y))
              continue;
        } else
        {
           if (!inRange(zy1, zy2, y))
              continue;
        }

        for (int x = 0; x < width; x++)
        {
            if (invertArea==1)
            {
               if (inRange(zx1, zx2, x))
                  continue;
            } else
            {
               if (!inRange(zx1, zx2, x))
                  continue;
            }

            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            img(x,y,0,0) = myBitmap[2 + o];
            img(x,y,0,1) = myBitmap[1 + o];
            img(x,y,0,2) = myBitmap[o];
            img(x,y,0,3) = myBitmap[3 + o];
        }
    }
}

DLL_API int DLL_CALLCONV cImgAddGaussianNoiseOnBitmap(unsigned char *imageData, int width, int height, int intensity, int Stride, int bpp) {
  int channels = (bpp==32) ? 4 : 3;
  CImg<unsigned char> img(imageData, channels, width, height, 1);
  img.permute_axes("yzcx");
  img.noise(intensity, 0);
  FillGdipLockedBitmapDataFromCImg(imageData, img, width, height, Stride, bpp);
  return 1;
}

DLL_API int DLL_CALLCONV cImgSharpenBitmap(unsigned char *imageData, int width, int height, int intensity, int Stride, int bpp) {
  int channels = (bpp==32) ? 4 : 3;
  CImg<unsigned char> img(imageData, channels, width, height, 1);
  img.permute_axes("yzcx");
  img.sharpen(intensity); // shock filters do not work?!
  FillGdipLockedBitmapDataFromCImg(imageData, img, width, height, Stride, bpp);
  return 1;
}

DLL_API int DLL_CALLCONV cImgBlurBitmapFilters(unsigned char *imageData, int width, int height, int intensityX, int intensityY, int modus, int circle, int preview, int Stride, int bpp) {
  int ow = width;  int oh = height;
  int channels = (bpp==32) ? 4 : 3;
  CImg<unsigned char> img(imageData, channels, width, height, 1);
  img.permute_axes("yzcx");
  if (preview==1)
  {
     width /=2;          height /=2;
     intensityX /=2;     intensityY /=2;
     img.resize(width,height, -100, -100, 3);
  }

  CImg<unsigned char> shape(intensityX,intensityX,1,3,0);
  const unsigned char clr[] = {254, 254, 254};
  if (circle==1)
  {
     shape.draw_circle(intensityX/2, intensityX/2, intensityX/2, clr);
     // shape.blur(3, 3, 0, 1, 3);
     if (intensityX!=intensityY)
        shape.resize(intensityX, intensityY);
  }

  if (modus==1)
  {
     img.blur_box(intensityX, intensityY, 0, 3);
  } else if (modus==2)
  {
     if (circle==1)
        img.dilate(shape);
     else
        img.dilate(intensityX, intensityY, 1);
  } else if (modus==3)
  {
     if (circle==1)
        img.erode(shape);
     else
        img.erode(intensityX, intensityY, 1);
  } else if (modus==4)
  {
     if (circle==1)
        img.opening(shape);
     else
        img.opening(intensityX, intensityY, 1);
  } else if (modus==5)
  {
     if (circle==1)
        img.closing(shape);
     else
        img.closing(intensityX, intensityY, 1);
  } else
  {
     img.blur(intensityX, intensityY, 0, 1, 3);
  }

  if (circle==1)
     img.blur(3, 3, 0, 1, 3);

  if (preview==1)
     img.resize(ow, oh, -100, -100, 3);

  FillGdipLockedBitmapDataFromCImg(imageData, img, ow, oh, Stride, bpp);
  return 1;
}

DLL_API Gdiplus::GpBitmap* DLL_CALLCONV cImgRotateBitmap(Gdiplus::GpBitmap *myBitmap, int width, int height, float angle, int interpolation, int bond) {
// function unused
  Gdiplus::GpBitmap *newBitmap = NULL;
  CImg<float> img(width,height,1,4);
  int r = FillCImgFromBitmap(img, myBitmap, width, height);
  if (r==0)
     return newBitmap;

  img.rotate(angle, interpolation, bond);
  newBitmap = CreateGdipBitmapFromCImg(img, img.width(), img.height());
  return newBitmap;
}

DLL_API Gdiplus::GpBitmap* DLL_CALLCONV cImgResizeBitmap(Gdiplus::GpBitmap *myBitmap, int width, int height, int resizedW, int resizedH, int interpolation, int bond) {
  Gdiplus::GpBitmap *newBitmap = NULL;
  CImg<float> img(width,height,1,4);
  int r = FillCImgFromBitmap(img, myBitmap, width, height);
  if (r==0)
     return newBitmap;

  img.resize(resizedW, resizedH, -100, -100, interpolation, bond);

  newBitmap = CreateGdipBitmapFromCImg(img, img.width(), img.height());
  return newBitmap;
}

DLL_API Gdiplus::GpBitmap* DLL_CALLCONV cImgLoadBitmap(const char *szFileName, int givenW, int givenH, int interpolation, int bond) {
// it does not work

  Gdiplus::GpBitmap *newBitmap = NULL;
  std::string ss = szFileName;
  fnOutputDebug("cimg file = " + ss);

  CImg<unsigned char> img;
  img.load_jpeg(szFileName);

  auto nSize = adaptImageGivenSize(1, 1, img.width(), img.height(), givenW, givenH);
  img.resize(nSize[0], nSize[1], -100, -100, interpolation, bond);

  CImg<float> img2 = img;
  newBitmap = CreateGdipBitmapFromCImg(img2, img.width(), img.height());

  return newBitmap;
}

DLL_API Gdiplus::GpBitmap* DLL_CALLCONV GenerateCIMGnoiseBitmap(int width, int height, int intensity, int details, int scale, int blurX, int blurY, int doBlur) {
  Gdiplus::GpBitmap *newBitmap = NULL;
  CImg<float> img(width,height,1,4);

  img.draw_plasma((float)intensity/2.0f, (float)details/2.0f, (float)scale/9.5f);
  if (doBlur==1)
     img.blur(blurX, blurY, 0, 1, 2);

  newBitmap = CreateGdipBitmapFromCImg(img, width, height);
  return newBitmap;
}

DLL_API int DLL_CALLCONV dissolveBitmap(int *imageData, int *newData, int Width, int Height, int rx, int ry) {
      // fnOutputDebug("maxR===" + std::to_string(maxuRadius) + "; rS=" + std::to_string(rScale) + "; imgAR=" + std::to_string(imgAR));
      const UINT maxPixels = Width + Height * Width;
      std::vector<bool> pixelzMap(maxPixels, 0);
      time_t nTime;
      srand((unsigned) time(&nTime));

      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < Height; y++)
      {
         for (int x = 0; x < Width; x++)
         {
            int gx = rand() % (rx*2) - rx;
            int gy = rand() % (ry*2) - ry;
            int dx = clamp(x + gx, 0, Width - 1);
            int dy = clamp(y + gy, 0, Height - 1);
            if (pixelzMap[dx + dy*Width]==1)
            {
               gx = rand() % (rx*2) - rx;
               gy = rand() % (ry*2) - ry;
               dx = clamp(x + gx, 0, Width - 1);
               dy = clamp(y + gy, 0, Height - 1);
            }

            pixelzMap[dx + dy*Width] = 1;
            newData[x + y*Width] = imageData[dx + (dy * Width)];
         }
      }

      if (rx>35 || ry>35)
      {
          #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
          for (int y = 0; y < Height; y++)
          {
             for (int x = 0; x < Width; x++)
             {
                int gx = rand() % (rx*2) - rx;
                int gy = rand() % (ry*2) - ry;
                int dx = clamp(x + gx, 0, Width - 1);
                int dy = clamp(y + gy, 0, Height - 1);
                if (pixelzMap[dx + dy*Width]==1)
                {
                   gx = rand() % (rx*2) - rx;
                   gy = rand() % (ry*2) - ry;
                   dx = clamp(x + gx, 0, Width - 1);
                   dy = clamp(y + gy, 0, Height - 1);
                }

                pixelzMap[dx + dy*Width] = 1;
                newData[x + y*Width] = imageData[dx + (dy * Width)];
            }
          }
      }

      if (rx>350 || ry>350)
      {
          #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
          for (int y = 0; y < Height; y++)
          {
             for (int x = 0; x < Width; x++)
             {
                int gx = rand() % (rx*2) - rx;
                int gy = rand() % (ry*2) - ry;
                int dx = clamp(x + gx, 0, Width - 1);
                int dy = clamp(y + gy, 0, Height - 1);
                if (pixelzMap[dx + dy*Width]==1)
                {
                   gx = rand() % (rx*2) - rx;
                   gy = rand() % (ry*2) - ry;
                   dx = clamp(x + gx, 0, Width - 1);
                   dy = clamp(y + gy, 0, Height - 1);
                }

                pixelzMap[dx + dy*Width] = 1;
                newData[x + y*Width] = imageData[dx + (dy * Width)];
            }
          }
      }

      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < Height; y++)
      {
         for (int x = 0; x < Width; x++)
         {
            if (pixelzMap[x + y*Width]!=1)
               newData[x + y*Width] = imageData[x + (y * Width)];
        }
      }
      return 1;
}

DLL_API int DLL_CALLCONV symmetricaBitmap(int *imageData, int Width, int Height, int rx, int ry) {
// only works with 32-ARGB bitmaps 

      if (rx>0)
      {
         #pragma omp parallel for schedule(static) default(none) // num_threads(3)
         for (int y = 0; y < Height; y++)
         {
            // fnOutputDebug("y=" + std::to_string(y) + "; rx=" + std::to_string(rx));
            int p = -1;
            for (int x = 0; x < Width; x++)
            {
               if (x>=rx)
               {
                  p++;
                  int gx = (p>=rx) ? clamp(x - rx*2, 0, Width - 1) : clamp(rx - p, 0, Width - 1);
                  // fnOutputDebug(std::to_string(p) + "=p; x="  +  std::to_string(x) + "; gx=" + std::to_string(gx));
                  imageData[x + y*Width] = imageData[gx + (y * Width)];
               }
            }
            p = 0;
         }
      }

      if (ry>0)
      {
         #pragma omp parallel for schedule(static) default(none) // num_threads(3)
         for (int x = 0; x < Width; x++)
         {
            // fnOutputDebug("y=" + std::to_string(y) + "; rx=" + std::to_string(rx));
            int p = -1;
            for (int y = 0; y < Height; y++)
            {
               if (y>=ry)
               {
                  p++;
                  int gy = (p>=ry) ? clamp(y - ry*2, 0, Height - 1) : clamp(ry - p, 0, Height - 1);
                  // fnOutputDebug(std::to_string(p) + "=p; x="  +  std::to_string(x) + "; gx=" + std::to_string(gx));
                  imageData[x + y*Width] = imageData[x + (gy * Width)];
               }
            }
            p = 0;
         }
      }

      return 1;
}

DLL_API int DLL_CALLCONV autoContrastBitmap(unsigned char *imageData, unsigned char *miniData, int Width, int Height, int mw, int mh, int modus, int intensity, int linearGamma, int Stride, int StrideMini, int bpp, unsigned char *maskBitmap, int mStride) {
      int maxRLevel = 0;      int minRLevel = 255;
      int maxGLevel = 0;      int minGLevel = 255;
      int maxBLevel = 0;      int minBLevel = 255;
      for (int y = 0; y < mh; y++)
      {
         for (int x = 0; x < mw; x++)
         {
            INT64 o = CalcPixOffset(x, y, StrideMini, bpp);
            if (bpp==32)
            {
               if (miniData[3 + o]<30)
                  continue;
            }

            if (modus==2)
            {
               int aR = miniData[2 + o]; // red
               int aB = miniData[o];     // blue
               maxRLevel = max(aR, maxRLevel);    minRLevel = min(aR, minRLevel);
               maxBLevel = max(aB, maxBLevel);    minBLevel = min(aB, minBLevel);
            }

            int aG = miniData[1 + o];   // green
            maxGLevel = max(aG, maxGLevel);    minGLevel = min(aG, minGLevel);
         }
      }

      if ((maxGLevel==minGLevel || maxGLevel==0 && minGLevel==255) && modus==1)
         return 1;

      maxGLevel -= minGLevel;
      double fG = 255.0f / maxGLevel;
      double fR = fG;
      double fB = fG;
      if (modus==2)
      {
         maxRLevel -= minRLevel;
         fR = 255.0f / maxRLevel;
         maxBLevel -= minBLevel;
         fB = 255.0f / maxBLevel;
      } else
      {
         minRLevel = minGLevel;
         minBLevel = minGLevel;
         maxRLevel = maxGLevel;
         maxBLevel = maxGLevel;
      }

      float fintensity = char_to_float[intensity];
      fnOutputDebug("RmaxL===" + std::to_string(maxRLevel) + "; minL=" + std::to_string(minRLevel) + "; fR=" + std::to_string(fR) + "; m=" + std::to_string(modus));
      fnOutputDebug("GmaxL===" + std::to_string(maxGLevel) + "; minL=" + std::to_string(minGLevel) + "; fG=" + std::to_string(fG)  );
      fnOutputDebug("BmaxL===" + std::to_string(maxBLevel) + "; minL=" + std::to_string(minBLevel) + "; fB=" + std::to_string(fB)  );
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int x = 0; x < Width; x++)
      {
         for (int y = 0; y < Height; y++)
         {
            if (clipMaskFilter(x, y, maskBitmap, mStride)==1)
               continue;

            INT64 o = CalcPixOffset(x, y, Stride, bpp);
            // fnOutputDebug("x===" + std::to_string(x) + "; y=" + std::to_string(y));
            int rO = imageData[2 + o];
            int gO = imageData[1 + o];
            int bO = imageData[o];
            int rB = clamp((int)round( (float)(rO - minRLevel)*fR ) , 0, 255);
            int gB = clamp((int)round( (float)(gO - minGLevel)*fG ) , 0, 255);
            int bB = clamp((int)round( (float)(bO - minBLevel)*fB ) , 0, 255);

            if (linearGamma==1 && intensity<255)
            {
               imageData[2 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[rB], gamma_to_linear[rO], fintensity)];
               imageData[1 + o] = linear_to_gamma[weighTwoValues(gamma_to_linear[gB], gamma_to_linear[gO], fintensity)];
               imageData[o]     = linear_to_gamma[weighTwoValues(gamma_to_linear[bB], gamma_to_linear[bO], fintensity)];
            } else
            {
               imageData[2 + o] = weighTwoValues(rB, rO, fintensity);
               imageData[1 + o] = weighTwoValues(gB, gO, fintensity);
               imageData[o]     = weighTwoValues(bB, bO, fintensity);
            }
            // imageData[x + (y * Width)] = (aO << 24) | (tR << 16) | (tG << 8) | tB;
         }
      }

      return 1;
}

DLL_API int DLL_CALLCONV rect2polarIMG(int *imageData, int *newData, int Width, int Height, double cx, double cy, double userScale) {
// inspired by https://imagej.nih.gov/ij/plugins/polar-transformer.html
      double maxuRadius = 0;
      double minBoundary = min(Width, Height);

      #pragma omp parallel for schedule(dynamic) default(none) shared(maxuRadius) // num_threads(3)
      for (int y = 0; y < Height; y++)
      {
         for (int x = 0; x < Width; x++)
         {
            double px = x - cx;     double py = y - cy;
            double r = sqrt(px*px + py*py);
            if (r<0)
               r = 0;

            maxuRadius = max(r, maxuRadius);
        }
      }

      double desiredRadius = minBoundary/2;
      double rScale = maxuRadius/desiredRadius;

      // fnOutputDebug("maxR===" + std::to_string(maxuRadius) + "; rS=" + std::to_string(rScale) + "; imgAR=" + std::to_string(imgAR));
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < Height; y++)
      {
         for (int x = 0; x < Width; x++)
         {
            double angle = 0;
            double px = x - cx;     double py = y - cy;
            double r = sqrt(px*px + py*py);
            // if ((y - cy)<=0)
            //    angle = 2*M_PI + atan2(y - cy, x - cx);
            // else
               angle = atan2(py, px);

            angle = rad2deg(angle) + 90;
            if (angle<0)
               angle += 360;
            // double zx = r*cos(angle);
            // double zy = r*sin(angle);
            int dx = (angle / 360.0f) * Width;
            int dy = (r / maxuRadius) * Height * rScale * userScale;
            dx = clamp(dx, 0, Width - 1);
            dy = clamp(dy, 0, Height - 1);
            newData[x + y*Width] = imageData[dx + (dy * Width)];
        }
      }
      return (int)maxuRadius;
}


DLL_API int DLL_CALLCONV polar2rectIMG(int *imageData, int *newData, int Width, int Height, double cx, double cy, double userScale) {
// TO-DO: this is an absolutely dumb implementation; I do not know how to optimize it
// inspired by https://imagej.nih.gov/ij/plugins/polar-transformer.html

      double maxuRadius = 0;
      double minBoundary = min(Width, Height);

      // identify the maximum radius
      // #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < Height*2; y++)
      {
         for (int x = 0; x < Width*2; x++)
         {
            double px = x/2.0f - cx;     double py = y/2.0f - cy;
            double r = sqrt(px*px + py*py);
            if (r<0)
               r = 0;

            maxuRadius = max(r, maxuRadius);
        }
      }

      double desiredRadius = minBoundary/2;
      double rScale = maxuRadius/desiredRadius;
      const UINT maxPixels = Width + Height * Width;
      // int *pixelzMap{ new int[maxPixels]{} };  // dynamically allocated array
      // memset(pixelzMap, -1, maxPixels * sizeof(int*)); // and fill it with -1
      // std::fill(pixelzMap, pixelzMap + maxPixels, -1); 
      std::vector<int> pixelzMap(maxPixels, -1);
      double imgAR = Width/Height;
      if (imgAR<1)
         imgAR = 1;

      // fill image with accuracy 2x, 4x, 8x, 16x and 32x
      // the upper sections of the image need high accuracy
      double acX = 1.0;
      double acY = 1.0;
      int ph = Height;
      int pw = Width;


      acX = 2.0;       acY = 2.0;
      ph = Height*acY; pw = Width*acX;
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < ph; y++)
      {
         // if (y>=ph*0.425)
         //   continue;
         for (int x = 0; x < pw; x++)
         {
            if (x>=pw*0.41 && x<=pw*0.59)
              continue;

            double angle = 0;
            double px = (double)x/acX - cx;     double py = (double)y/acY - cy;
            double r = sqrt(px*px + py*py);
            // if ((y - cy)<=0)
            //    angle = 2*M_PI + atan2(y - cy, x - cx);
            // else
               angle = atan2(py, px);

            angle = rad2deg(angle) + 90;
            if (angle<0)
               angle += 360;

            int dx = (angle / 360.0f) * Width;
            int dy = (r / maxuRadius) * Height * rScale * userScale;
            dx = clamp(dx, 0, Width - 1);
            dy = clamp(dy, 0, Height - 1);

            int zx = x/acX; int zy = y/acY;
            zx = clamp(zx, 0, Width - 1);
            zy = clamp(zy, 0, Height - 1);

            pixelzMap[dx + dy*Width] = zx;
            newData[dx + dy*Width] = imageData[zx + (zy * Width)];
        }
      }

      acX = 4.0;       acY = 4.0;
      ph = Height*acY; pw = Width*acX;
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < ph; y++)
      {
         // if (y>ph*0.455)
         //   continue;
         for (int x = 0; x < pw; x++)
         {
            if (x<pw*0.4)
              continue;
            if (x>pw*0.6)
              continue;

            double px = (double)x/acX - cx;     double py = (double)y/acY - cy;
            double r = sqrt(px*px + py*py);
            double angle = atan2(py, px);
            angle = rad2deg(angle) + 90;
            if (angle<0)
               angle += 360;

            int dx = (angle / 360.0f) * Width;
            int dy = (r / maxuRadius) * Height * rScale * userScale;
            dx = clamp(dx, 0, Width - 1);
            dy = clamp(dy, 0, Height - 1);

            int zx = x/acX; int zy = y/acY;
            zx = clamp(zx, 0, Width - 1);
            zy = clamp(zy, 0, Height - 1);

            pixelzMap[dx + dy*Width] = zx;
            newData[dx + dy*Width] = imageData[zx + (zy * Width)];
        }
      }

      acX = 8.0;       acY = 8.0;
      ph = Height*acY; pw = Width*acX;
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < ph; y++)
      {
         if (y<ph*0.45)
           continue;
         for (int x = 0; x < pw; x++)
         {
            if (x<pw*0.45)
              continue;
            if (x>pw*0.55)
              continue;

            double px = (double)x/acX - cx;     double py = (double)y/acY - cy;
            double r = sqrt(px*px + py*py);
            double angle = atan2(py, px);
            angle = rad2deg(angle) + 90;
            if (angle<0)
               angle += 360;

            int dx = (angle / 360.0f) * Width;
            int dy = (r / maxuRadius) * Height * rScale * userScale;
            dx = clamp(dx, 0, Width - 1);
            dy = clamp(dy, 0, Height - 1);

            int zx = x/acX; int zy = y/acY;
            zx = clamp(zx, 0, Width - 1);
            zy = clamp(zy, 0, Height - 1);

            pixelzMap[dx + dy*Width] = zx;
            newData[dx + dy*Width] = imageData[zx + (zy * Width)];
        }
      }

      acX = 16.0;      acY = 16.0;
      ph = Height*acY; pw = Width*acX;
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < ph; y++)
      {
         if (y<ph*0.47)
           continue;
         for (int x = 0; x < pw; x++)
         {
            if (x<pw*0.47)
              continue;
            if (x>pw*0.53)
              continue;

            double px = (double)x/acX - cx;     double py = (double)y/acY - cy;
            double r = sqrt(px*px + py*py);
            double angle = atan2(py, px);
            angle = rad2deg(angle) + 90;
            if (angle<0)
               angle += 360;

            int dx = (angle / 360.0f) * Width;
            int dy = (r / maxuRadius) * Height * rScale * userScale;
            dx = clamp(dx, 0, Width - 1);
            dy = clamp(dy, 0, Height - 1);

            int zx = x/acX; int zy = y/acY;
            zx = clamp(zx, 0, Width - 1);
            zy = clamp(zy, 0, Height - 1);

            pixelzMap[dx + dy*Width] = zx;
            newData[dx + dy*Width] = imageData[zx + (zy * Width)];
        }
      }

      acX = 32.0;       acY = 32.0;
      ph = Height*acY;  pw = Width*acX;
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 0; y < ph; y++)
      {
         if (y<ph*0.49)
           continue;
         for (int x = 0; x < pw; x++)
         {
            if (x<pw*0.49)
              continue;
            if (x>pw*0.51)
              continue;

            double px = (double)x/acX - cx;     double py = (double)y/acY - cy;
            double r = sqrt(px*px + py*py);
            double angle = atan2(py, px);
            angle = rad2deg(angle) + 90;
            if (angle<0)
               angle += 360;

            int dx = (angle / 360.0f) * Width;
            int dy = (r / maxuRadius) * Height * rScale * userScale;
            dx = clamp(dx, 0, Width - 1);
            dy = clamp(dy, 0, Height - 1);

            int zx = x/acX; int zy = y/acY;
            zx = clamp(zx, 0, Width - 1);
            zy = clamp(zy, 0, Height - 1);

            pixelzMap[dx + dy*Width] = zx;
            newData[dx + dy*Width] = imageData[zx + (zy * Width)];
        }
      }

      int failed = 0; int fixt = 0;
      // fill missing pixels; silly algorithm
      #pragma omp parallel for schedule(dynamic) default(none) // num_threads(3)
      for (int y = 1; y < Height - 1; y++)
      {
         for (int x = 1; x < Width - 1; x++)
         {
               int px = x + y*Width;
               if (pixelzMap[px]==-1)
               {
                   fixt++;
                   if (pixelzMap[x-1 + y*Width]==(x-1))
                   {
                      pixelzMap[px] = pixelzMap[x-1 + y*Width];
                      newData[px] = newData[x-1 + (y * Width)];
                   } else if (pixelzMap[x+1 + y*Width]==(x+1))
                   {
                      pixelzMap[px] = pixelzMap[x+1 + y*Width];
                      newData[px] = newData[x+1 + (y * Width)];
                   } else if (pixelzMap[x + (y-1)*Width]==x)
                   {
                      pixelzMap[px] = pixelzMap[x + (y-1)*Width];
                      newData[px] = newData[x + ((y-1) * Width)];
                   } else if (pixelzMap[x + (y+1)*Width]==x)
                   {
                      pixelzMap[px] = pixelzMap[x + (y+1)*Width];
                      newData[px] = newData[x + ((y+1) * Width)];
                   } else if (pixelzMap[x + (y-1)*Width]>=0)
                   {
                      pixelzMap[px] = pixelzMap[x + (y-1)*Width];
                      newData[px] = newData[x + ((y-1) * Width)];
                   } else if (pixelzMap[x + (y+1)*Width]>=0)
                   {
                      pixelzMap[px] = pixelzMap[x + (y+1)*Width];
                      newData[px] = newData[x + ((y+1) * Width)];
                   } else if (pixelzMap[x-1 + y*Width]>=0)
                   {
                      pixelzMap[px] = pixelzMap[x-1 + y*Width];
                      newData[px] = newData[x-1 + (y * Width)];
                   } else if (pixelzMap[x+1 + y*Width]>=0)
                   {
                      pixelzMap[px] = pixelzMap[x+1 + y*Width];
                      newData[px] = newData[x+1 + (y * Width)];
                   } else 
                      failed++;
               }
        }
      }

      fixt -= failed;
      // fnOutputDebug("failed= " + std::to_string(failed) + " yay= " + std::to_string(fixt));
      int y = 0;
      for (int x = 1; x < Width - 1; x++)
      {
          int px = x + y*Width;
          if (pixelzMap[px]==-1)
          {
              fixt++;
              pixelzMap[px] = pixelzMap[x-1 + y*Width];
              newData[px] = newData[x-1 + (y * Width)];
          }
      }

      y = Height - 1;
      for (int x = 1; x < Width - 1; x++)
      {
          int px = x + y*Width;
          if (pixelzMap[px]==-1)
          {
              fixt++;
              pixelzMap[px] = pixelzMap[x-1 + y*Width];
              newData[px] = newData[x-1 + (y * Width)];
          }
      }

      // delete[] pixelzMap;
      return (int)maxuRadius;
}

DLL_API int DLL_CALLCONV SetTabletPenServiceProperties(HWND hWnd) {
    // https://learn.microsoft.com/en-us/windows/win32/tablet/wm-tablet-querysystemgesturestatus-message
    ATOM atom = ::GlobalAddAtom(MICROSOFT_TABLETPENSERVICE_PROPERTY);    
    ::SetProp(hWnd, MICROSOFT_TABLETPENSERVICE_PROPERTY, reinterpret_cast<HANDLE>(dwHwndTabletProperty));
    ::GlobalDeleteAtom(atom);
    return 1;
}        


/*
DLL_API int DLL_CALLCONV STBresizeBitmap(const unsigned char *bitmapDatu, int w, int h, int Stride, unsigned char *otherData, int w2, int h2, int mStride) { 
  unsigned char *r = stbir_resize_uint8_linear(bitmapDatu, w, h, Stride, otherData, w2, h2, mStride, STBIR_BGRA);
  return 1;
}
DLL_API Gdiplus::GpBitmap* DLL_CALLCONV testCimgQPV(Gdiplus::GpBitmap *myBitmap, int width, int height, int intensityX, int intensityY, int modus) {

    // Load input image files, using the CImg Library.
    Gdiplus::GpBitmap *newBitmap = NULL;
    CImg<float> img(width,height,1,4);
    int r = FillCImgFromBitmap(img, myBitmap, width, height);
    if (r==0)
       return newBitmap;

    // Move input images into a list.
    CImgList<float> image_list;
    img.move_to(image_list);

    // Define the corresponding image names.
    CImgList<char> image_names;
    CImg<char>::string("First image").move_to(image_names);

    // Invoke libgmic to execute a G'MIC pipeline.
    gmic("blur_angular 2% name \"Result image\"", image_list, image_names);

    // Display resulting image.
    const char *const title_bar = image_names[0];
    image_list.display(title_bar);

    newBitmap = CreateGdipBitmapFromCImg(img, width, height);
    return newBitmap;
}


DLL_API Gdiplus::GpBitmap* DLL_CALLCONV testOtherCimgQPV(Gdiplus::GpBitmap *myBitmap, int width, int height, int intensityX, int intensityY, int modus) {
  // width = 129;
  // height = 129;
  // CImg<float> img(129,129,1,3,"0,64,128,192,255",true); // Construct image from a value sequence
  //             img2(129,129,1,3,"if(c==0,255*abs(cos(x/10)),1.8*y)",false); // Construct image from a formula
  // (img1,img2).display();

  Gdiplus::GpBitmap *newBitmap = NULL;
  CImg<float> img(width,height,1,4);
  int r = FillCImgFromBitmap(img, myBitmap, width, height);
  if (r==0)
     return newBitmap;
  char k = 'x';

  // if (modus==1)
  //    img.blur_median(intensityX, 3);   // size, distance threshold
  if (modus==2)
     img.blur_guided(img, intensityX, intensityY/2);     // guide, radius, regularization
  // else if (modus==3)
  //    img.vanvliet(intensityX, 1, k);       // sigma, order, axis
  else if (modus==4)
     img.dilate(intensityX, intensityY, 1);
  else if (modus==5)
     img.erode(intensityX, intensityY, 1);
  else if (modus==6)
     img.opening(intensityX, intensityY, 1);
  else if (modus==7)
     img.closing(intensityX, intensityY, 1);
  else if (modus==8)
     img.normalize(intensityX, intensityY);  // min.val , max.val
  else if (modus==9)
     img.equalize(255, intensityX, intensityY);   // nr.levels , min.val , max.val
  else if (modus==10)
     img.threshold(intensityX);         // value, soft, strict
  else if (modus==11)
     img.quantize(intensityX, 1);   // nr.levels, keepRange
  else if (modus==12)
     img.warp(img);          /// img, mode, interpolation
  else if (modus==13)
     img.watershed(img);
  else
     return newBitmap;

// img.display();

// warp()
// Vanvliet()
// edges()
// deriche()
// blur_angular()
// blur_radial()
// blur_linear()
// deblur()
// syntexturize()
// sharpen()
// watershed()
// noise_perlin()
// rorschach()
// turbulence()
// polka_dots()
// voronoi()
// maze()
// mosaic()
// boxfitting()
// fractalize()
// houghsketchbw()
// stained_glass()
// stencil()
// cubism()
// sponge()
// deform()
// kaleidoscope()
// twirl()
// ripple()
// water()
// spherize()
// fisheye()
// wave()
// wind()
// raindrops()

  newBitmap = CreateGdipBitmapFromCImg(img, width, height);
  // ~Cimg(img);
  return newBitmap;
}

*/


/*
DLL_API Gdiplus::GpBitmap* DLL_CALLCONV testFunc(UINT width, UINT height, const wchar_t *szFileName) {
    Gdiplus::GpBitmap* myBitmap = NULL;
    Gdiplus::DllExports::GdipCreateBitmapFromFile(szFileName, &myBitmap);
    // Gdiplus::DllExports::GdipCreateBitmapFromScan0(width, height, 0, PixelFormat32bppRGB, NULL, &myBitmap);

    std::wstring string_to_convert(szFileName);

    //setup converter
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

    //use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
    std::string converted_str = converter.to_bytes( string_to_convert );

    std::stringstream ss;
    ss << "qpv: bmp file " << converted_str;
    OutputDebugStringA(ss.str().data());
    return myBitmap;
}
*/


/*
DLL_API int DLL_CALLCONV testFuncNow(int givenQuality, UINT width, UINT height, const wchar_t *szFileName, const wchar_t *szFileNameOut) {
  //  VImage in = VImage::new_from_file (szFileName,
  //   VImage::option ()->set ("access", VIPS_ACCESS_SEQUENTIAL));

  // double avg = in.avg ();

  // printf ("avg = %g\n", avg);
  // printf ("width = %d\n", in.width ());
  in = VImage::new_from_file (szFileName,
    VImage::option ()->set ("access", VIPS_ACCESS_SEQUENTIAL));

  VImage out = in.embed (10, 10, 1000, 1000,
    VImage::option ()->
      set ("extend", "background")->
      set ("background", 128));

  out.write_to_file (szFileNameOut);

  vips_shutdown ();
  return 1
}
*/


/*
DLL_API int DLL_CALLCONV BlurImage(unsigned char *Bitmap, int width, int height, int Stride, int radius) {
// https://stackoverflow.com/questions/47209262/c-blur-effect-on-bit-map-is-working-but-colors-are-changed

    float rs = ceil(radius * 2.57);
    for (int i = 0; i < height; ++i)
    {
        for (int j = 0; j < width; ++j)
        {
            int tempCoord;
            double a = 0, r = 0, g = 0, b = 0;
            double count = 0;

            for (int iy = i - rs; iy < i + rs + 1; ++iy)
            {
                for (int ix = j - rs; ix < j + rs + 1; ++ix)
                {
                    auto x = min(width - 1, max(0, ix));
                    auto y = min(height - 1, max(0, iy));

                    auto dsq = ((ix - j) * (ix - j)) + ((iy - i) * (iy - i));
                    float wght = std::exp(-dsq / (2.0 * radius * radius)) / (M_PI * 2.0 * radius * radius);

                    tempCoord = (4 * x) + y*Stride;
                    a = Bitmap[3 + tempCoord] * wght;
                    r = Bitmap[2 + tempCoord] * wght;
                    g = Bitmap[1 + tempCoord] * wght;
                    b = Bitmap[tempCoord] * wght;
                    // rgb32* pixel = bmp->getPixel(x, y);
                    // r += pixel->r * wght;
                    // g += pixel->g * wght;
                    // b += pixel->b * wght;
                    count += wght;
                }
            }

            tempCoord = (4 * j) + i*Stride;
            // if a
               Bitmap[3 + tempCoord] = round(a);
            // if r
               Bitmap[2 + tempCoord] = round(r);
            // if g
               Bitmap[1 + tempCoord] = round(g);
            // if b
               Bitmap[tempCoord] = round(b);
            // rgb32* pixel = bmp->getPixel(j, i);
            // pixel->r = std::round(r / count);
            // pixel->g = std::round(g / count);
            // pixel->b = std::round(b / count);
        }
    }
    return 1;
}

DLL_API int DLL_CALLCONV hammingDistance(char str1[], char str2[])
{
    int i = 0, count = 0;
    while(str1[i]!='\0')
    {
        if (str1[i] != str2[i])
            count++;
        i++;
    }
    return count;
}

DLL_API int DLL_CALLCONV hamming_distance(int* a, int*b, int n) {
    int dist = 0;
    std::stringstream ss;
    ss << "qpv: a " << a;
    ss << "qpv: b " << b;

    for(int i=0; i<n; i++) {
        if (a[i] != b[i])
           dist++;
    }
    return dist;
}

// For every integer on 8bits (from 0 to 255), keep track of how many
// bits are set in the binary representation of that integer.
// Note, the only 8bit integer that has all 8 bits set is 255 (last element here)
// and 0 is the only element with no bits set.
const std::vector<int> globalNumberOfBits = {
0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

// Used once, to populate the variable above.
void GenerateCodeFor_globalNumberOfBits() {
  printf("const std::vector<int> globalNumberOfBits = {\n");
  for (int i = 0; i < 256; ++i) {
    int count = 0;
    for (int bit = 0; bit < 8; ++bit) {
      if (i & (1 << bit)) count++;
    }
    printf("%d, ", count);
    if (i % 16 == 15) printf("\n");
  }
  printf("};\n");
}

int CountDifferentBits(int64_t a, int64_t b) {
  int nr_diff_bits = 0;
  int64_t diffs = a ^ b;
  for (int i = 0; i < sizeof(int64_t); ++i) {
    const int last_8_bits = diffs & 0xFF;
    diffs = diffs >> 8;
    nr_diff_bits += globalNumberOfBits[last_8_bits];
  }
  return nr_diff_bits;
}

int CountDifferentBits(int64_t *img1, int64_t *img2, int nr_values) {
  int n_diff = 0;
  for (int i = 0; i < nr_values; ++i) {
    n_diff += CountDifferentBits(img1[i], img2[i]);
  }
  return n_diff;
}

unsigned long long int fact(UINT n) {
   if (n == 0 || n == 1)
      return 1;

   unsigned long long int r = n;
   for (unsigned long long int i = 1; i < n; i++)
   {
       r *= i;
   }
   return r;
}

DLL_API unsigned long long int DLL_CALLCONV dumbcalculateNCR(UINT n) {
// Calculates the number of combinations of N things taken r=2 at a time.

   // std::stringstream ss;
   // ss << "qpv: n=" << n;
 
   unsigned long long int combinations = 0;
   for ( unsigned long long int secondIndex = 0 ; secondIndex<n+1 ; secondIndex++)
   {
       for ( unsigned long long int mainIndex = secondIndex + 1 ; mainIndex<n ; mainIndex++)
       {
          // if (secondIndex!=mainIndex && secondIndex<n && mainIndex<n) 
          // {
             // std::stringstream ss;
             // ss << "qpv: sI=" << secondIndex;
             // ss << " mI=" << mainIndex;
             // OutputDebugStringA(ss.str().data());
             combinations++;
          // }
       }
   }

   // std::stringstream ss;
   // ss << " combos=" << combinations;
   // OutputDebugStringA(ss.str().data());
   return combinations;
}


UINT64 reverse_8bits(UINT64 n) {
  for (int i = 0, bytes = sizeof(n); i < bytes; ++i) {
    const int bit_offset = i * 8;
    for (int j = 0; j < 4; ++j) {
      // Calculam care e bit de schimbat, in cadrul la byte current (situat
      // la bit_offset, pe spatiu de 8 bits, numerotati de la 0 la 7.
      const int bit = bit_offset + j;
      const int sym_bit = bit_offset + 7 - j;

      // testam daca e setat bit
      // 1 << i face shift to left la bitul 1 cu i bits.
      const bool bit_s = (n & (1 << bit)) ? 1 : 0;

      // testam daca e setat bit sym_bit
      const bool bit_sym_s = (n & (1 << sym_bit)) ? 1 : 0;

      // Daca bit e setat, seteaza sym_i in loc (sau sterge)
      if (bit_s) {
        // bitwise or pe bit sym_i
        n |= 1 << sym_bit;
      } else {
        // bitwise and cu un numar care e
        // doar 1, si un singur 0 pe pozitia sym_bit (~ face negare)
        n &= ~(1 << sym_bit);
      }

      // La fel ca mai sus, la bit.
      if (bit_sym_s) {
        n |= 1 << bit;
      } else {
        n &= ~(1 << bit);
      }
    }
  }
  return n;
}

UINT64 revBits(UINT64 n){
  return (
     n    &0x0101010101010101 |  n>>5&0x0202020202020202 |  n>>3&0x0404040404040404 | n>>1&0x0808080808080808
  & ~n<< 1&0x1010101010101010 & ~n<<3&0x2020202020202020 & ~n<<5&0x4040404040404040 | n   &0x8080808080808080
  );
}

UINT64 revBits_entire(UINT64 n){
  return (
    n>> 7&0x0101010101010101 | n>>5&0x0202020202020202 | n>>3&0x0404040404040404 | n>>1&0x0808080808080808
  | n<< 1&0x1010101010101010 | n<<3&0x2020202020202020 | n<<5&0x4040404040404040 | n<<7&0x8080808080808080
  );
}


int reverse_bits(int n) {
  for (int i = 0, bits = sizeof(n) * 8; i < bits / 2; ++i)
  {
      // punctul simetric la i
      const int sym_i = bits - i - 1;

      // testam daca e setat bit i
      // 1 << i face shift to left la bitul 1 cu i bits.
      const bool bit_i = (n & (1 << i)) ? 1 : 0;

      // testam daca e setat bit sym_i
      const bool bit_sym_i = (n & (1 << sym_i)) ? 1 : 0;

      // Daca i e setat, seteaza sym_i in loc (sau sterge)
      if (bit_i)
      {
         // bitwise or pe bit sym_i
         n |= 1 << sym_i;
      } else
      {
         // bitwise and cu un numar care e
         // doar 1, si un singur 0 pe pozitia sym_i (~ face negare)
         n &= ~(1 << sym_i);
      }

      // La fel ca mai sus, la bit i.
      if (bit_sym_i)
      {
         n |= 1 << i;
      } else {
         n &= ~(1 << i);
      }
  }
  return n;
}
*/
