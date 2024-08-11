#include "bitmap.h"

OSD_COMP_INFO s_OSDCompInfo[OSD_COLOR_FMT_BUTT] = {
    {0, 4, 4, 4}, /*RGB444*/
    {4, 4, 4, 4}, /*ARGB4444*/
    {0, 5, 5, 5}, /*RGB555*/
    {0, 5, 6, 5}, /*RGB565*/
    {1, 5, 5, 5}, /*ARGB1555*/
    {0, 0, 0, 0}, /*RESERVED*/
    {0, 8, 8, 8}, /*RGB888*/
    {8, 8, 8, 8}  /*ARGB8888*/
};


static inline unsigned short convert_2bpp(unsigned char r, unsigned char g, unsigned char b, OSD_COMP_INFO compinfo)
{
    unsigned short pixel = 0;
    unsigned int tmp = 15;

    unsigned char r1 = r >> (8 - compinfo.rlen);
    unsigned char g1 = g >> (8 - compinfo.glen);
    unsigned char b1 = b >> (8 - compinfo.blen);
    while (compinfo.alen)
    {
        pixel |= (1 << tmp);
        tmp--;
        compinfo.alen--;
    }

    pixel |= (r1 | (g1 << compinfo.blen) | (b1 << (compinfo.blen + compinfo.glen)));
    return pixel;
}

int parse_bitmap(const char *filename, OSD_BITMAPFILEHEADER *pBmpFileHeader, OSD_BITMAPINFO *pBmpInfo)
{
    FILE *pFile;
    unsigned short bfType;

    if (!filename)
    {
        fprintf(stderr, "load_bitmap: filename=NULL\n");
        return -1;
    }

    if ((pFile = fopen(filename, "rb")) == NULL)
    {
        fprintf(stderr, "Open file faild:%s!\n", filename);
        return -1;
    }

    if (fread(&bfType, 1, sizeof(bfType), pFile) != sizeof(bfType))
    {
        fprintf(stderr, "fread file failed:%s!\n", filename);
        fclose(pFile);
        return -1;
    }

    if (bfType != 0x4d42)
    {
        fprintf(stderr, "not bitmap file\n");
        fclose(pFile);
        return -1;
    }

    if (fread(pBmpFileHeader, 1, sizeof(OSD_BITMAPFILEHEADER), pFile) != sizeof(OSD_BITMAPFILEHEADER))
    {
        fprintf(stderr, "fread OSD_BITMAPFILEHEADER failed:%s!\n", filename);
        fclose(pFile);
        return -1;
    }

    if (fread(pBmpInfo, 1, sizeof(OSD_BITMAPINFO), pFile) != sizeof(OSD_BITMAPINFO))
    {
        fprintf(stderr, "fread OSD_BITMAPINFO failed:%s!\n", filename);
        fclose(pFile);
        return -1;
    }

    if (pBmpInfo->bmiHeader.biBitCount / 8 < 2)
    {
        fprintf(stderr, "bitmap format not supported!\n");
        fclose(pFile);
        return -1;
    }

    if (pBmpInfo->bmiHeader.biCompression != 0 && pBmpInfo->bmiHeader.biCompression != 3)
    {
        fprintf(stderr, "not support compressed bitmap file!\n");
        fclose(pFile);
        return -1;
    }

    if (pBmpInfo->bmiHeader.biHeight < 0)
    {
        fprintf(stderr, "bmpInfo.bmiHeader.biHeight < 0\n");
        fclose(pFile);
        return -1;
    }

    fclose(pFile);
    return 0;
}

int load_bitmap(const char *filename, OSD_LOGO_T *pVideoLogo)
{
    FILE *pFile;

    unsigned int w, h, stride;
    unsigned short Bpp, dstBpp;

    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    unsigned char *pOrigBMPBuf;
    unsigned char *pRGBBuf;

    if (parse_bitmap(filename, &bmpFileHeader, &bmpInfo) < 0)
        return -1;

    if (!(pFile = fopen(filename, "rb")))
    {
        fprintf(stderr, "Open file faild:%s!\n", filename);
        return -1;
    }

    pVideoLogo->width = (unsigned short)bmpInfo.bmiHeader.biWidth;
    pVideoLogo->height = (unsigned short)((bmpInfo.bmiHeader.biHeight > 0) ? bmpInfo.bmiHeader.biHeight : (-bmpInfo.bmiHeader.biHeight));
    w = pVideoLogo->width;
    h = pVideoLogo->height;

    Bpp = bmpInfo.bmiHeader.biBitCount / 8;
    stride = w * Bpp;
    if (stride % 4)
        stride = (stride & 0xfffc) + 4;

    /* RGB8888 or RGB1555 */
    pOrigBMPBuf = (unsigned char *)malloc(h * stride);
    if (!pOrigBMPBuf)
    {
        fprintf(stderr, "not enough memory to malloc!\n");
        fclose(pFile);
        return -1;
    }

    pRGBBuf = pVideoLogo->pRGBBuffer;

    if (fseek(pFile, bmpFileHeader.bfOffBits, 0))
    {
        fprintf(stderr, "fseek failed!\n");
        fclose(pFile);
        free(pOrigBMPBuf);
        pOrigBMPBuf = NULL;
        return -1;
    }

    if (fread(pOrigBMPBuf, 1, (unsigned int)(h * stride), pFile) != (unsigned int)(h * stride))
    {
        fprintf(stderr, "fread (%d*%d)error!line:%d\n", h, stride, __LINE__);
        perror("fread:");
    }

    if (Bpp > 2)
        dstBpp = 4;
    else
        dstBpp = 2;

    if (pVideoLogo->stride == 0)
        pVideoLogo->stride = pVideoLogo->width * dstBpp;

    for (unsigned short i = 0; i < h; i++)
    {
        for (unsigned short j = 0; j < w; j++)
        {
            memcpy(pRGBBuf + i * pVideoLogo->stride + j * dstBpp, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);

            if (dstBpp == 4)
                *(pRGBBuf + i * pVideoLogo->stride + j * dstBpp + 3) = 0x80; /*alpha*/
        }
    }

    free(pOrigBMPBuf);
    pOrigBMPBuf = NULL;

    fclose(pFile);
    return 0;
}

int load_bitmapex(const char *filename, OSD_LOGO_T *pVideoLogo, OSD_COLOR_FMT_E enFmt)
{
    FILE *pFile;

    unsigned int w, h, stride;
    unsigned short Bpp;

    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    unsigned char *pOrigBMPBuf;
    unsigned char *pRGBBuf;
    unsigned char r, g, b;
    unsigned char *pStart;
    unsigned short *pDst;

    if (parse_bitmap(filename, &bmpFileHeader, &bmpInfo) < 0)
        return -1;

    if (!(pFile = fopen(filename, "rb")))
    {
        fprintf(stderr, "Open file failed:%s!\n", filename);
        return -1;
    }

    pVideoLogo->width = (unsigned short)bmpInfo.bmiHeader.biWidth;
    pVideoLogo->height = (unsigned short)((bmpInfo.bmiHeader.biHeight > 0) ? bmpInfo.bmiHeader.biHeight : (-bmpInfo.bmiHeader.biHeight));
    w = pVideoLogo->width;
    h = pVideoLogo->height;

    Bpp = bmpInfo.bmiHeader.biBitCount / 8;
    stride = w * Bpp;
    if (stride % 4)
        stride = (stride & 0xfffc) + 4;

    /* RGB8888 or RGB1555 */
    pOrigBMPBuf = (unsigned char *)malloc(h * stride);
    if (!pOrigBMPBuf)
    {
        fprintf(stderr, "not enough memory to malloc!\n");
        fclose(pFile);
        return -1;
    }

    pRGBBuf = pVideoLogo->pRGBBuffer;

    if (fseek(pFile, bmpFileHeader.bfOffBits, 0))
    {
        fprintf(stderr, "fseek failed!\n");
        fclose(pFile);
        free(pOrigBMPBuf);
        pOrigBMPBuf = NULL;
        return -1;
    }

    if (fread(pOrigBMPBuf, 1, (unsigned int)(h * stride), pFile) != (unsigned int)(h * stride))
    {
        fprintf(stderr, "fread (%d*%d)error!line:%d\n", h, stride, __LINE__);
        perror("fread:");
    }

    if (enFmt >= OSD_COLOR_FMT_RGB888)
        pVideoLogo->stride = pVideoLogo->width * 4;
    else
        pVideoLogo->stride = pVideoLogo->width * 2;

    for (unsigned short i = 0; i < h; i++)
    {
        for (unsigned short j = 0; j < w; j++)
        {
            if (Bpp == 3) /*.....*/
            {
                switch (enFmt)
                {
                case OSD_COLOR_FMT_RGB444:
                case OSD_COLOR_FMT_RGB555:
                case OSD_COLOR_FMT_RGB565:
                case OSD_COLOR_FMT_RGB1555:
                case OSD_COLOR_FMT_RGB4444:
                    /* start color convert */
                    pStart = pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp;
                    pDst = (unsigned short *)(pRGBBuf + i * pVideoLogo->stride + j * 2);
                    r = *(pStart);
                    g = *(pStart + 1);
                    b = *(pStart + 2);
                    *pDst = convert_2bpp(r, g, b, s_OSDCompInfo[enFmt]);
                    break;

                case OSD_COLOR_FMT_RGB888:
                case OSD_COLOR_FMT_RGB8888:
                    memcpy(pRGBBuf + i * pVideoLogo->stride + j * 4, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);
                    *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 3) = 0xff; /*alpha*/
                    break;

                default:
                    fprintf(stderr, "file(%s), line(%d), no such format!\n", __FILE__, __LINE__);
                    break;
                }
            }
            else if ((Bpp == 2) || (Bpp == 4)) /*..............*/
            {
                memcpy(pRGBBuf + i * pVideoLogo->stride + j * Bpp, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);
            }
        }
    }

    free(pOrigBMPBuf);
    pOrigBMPBuf = NULL;

    fclose(pFile);
    return 0;
}

int load_bitmap_canvas(const char *filename, OSD_LOGO_T *pVideoLogo, OSD_COLOR_FMT_E enFmt)
{
    FILE *pFile;

    unsigned int w, h, stride;
    unsigned short Bpp;

    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    unsigned char *pOrigBMPBuf;
    unsigned char *pRGBBuf;
    unsigned char r, g, b;
    unsigned char *pStart;
    unsigned short *pDst;

    if (parse_bitmap(filename, &bmpFileHeader, &bmpInfo) < 0)
        return -1;

    if (!(pFile = fopen(filename, "rb")))
    {
        fprintf(stderr, "Open file faild:%s!\n", filename);
        return -1;
    }

    Bpp = bmpInfo.bmiHeader.biBitCount / 8;
    w = (unsigned short)bmpInfo.bmiHeader.biWidth;
    h = (unsigned short)((bmpInfo.bmiHeader.biHeight > 0) ? bmpInfo.bmiHeader.biHeight : (-bmpInfo.bmiHeader.biHeight));

    stride = w * Bpp;
    if (stride % 4)
        stride = (stride & 0xfffc) + 4;

    /* RGB8888 or RGB1555 */
    pOrigBMPBuf = (unsigned char *)malloc(h * stride);
    if (!pOrigBMPBuf)
    {
        fprintf(stderr, "not enough memory to malloc!\n");
        fclose(pFile);
        return -1;
    }

    pRGBBuf = pVideoLogo->pRGBBuffer;

    if (stride > pVideoLogo->stride)
    {
        fprintf(stderr, "Bitmap's stride(%d) is bigger than canvas's stide(%d). Load bitmap error!\n", stride, pVideoLogo->stride);
        fclose(pFile);
        free(pOrigBMPBuf);
        return -1;
    }

    if (h > pVideoLogo->height)
    {
        fprintf(stderr, "Bitmap's height(%d) is bigger than canvas's height(%d). Load bitmap error!\n", h, pVideoLogo->height);
        fclose(pFile);
        free(pOrigBMPBuf);
        return -1;
    }

    if (w > pVideoLogo->width)
    {
        fprintf(stderr, "Bitmap's width(%d) is bigger than canvas's width(%d). Load bitmap error!\n", w, pVideoLogo->width);
        fclose(pFile);
        free(pOrigBMPBuf);
        return -1;
    }

    if (fseek(pFile, bmpFileHeader.bfOffBits, 0))
    {
        fprintf(stderr, "fseek error!\n");
        fclose(pFile);
        free(pOrigBMPBuf);
        return -1;
    }

    if (fread(pOrigBMPBuf, 1, (unsigned int)(h * stride), pFile) != (unsigned int)(h * stride))
    {
        fprintf(stderr, "fread (%d*%d)error!line:%d\n", h, stride, __LINE__);
        perror("fread:");
    }

    for (unsigned short i = 0; i < h; i++)
    {
        for (unsigned short j = 0; j < w; j++)
        {
            if (Bpp == 3) /*.....*/
            {
                switch (enFmt)
                {
                case OSD_COLOR_FMT_RGB444:
                case OSD_COLOR_FMT_RGB555:
                case OSD_COLOR_FMT_RGB565:
                case OSD_COLOR_FMT_RGB1555:
                case OSD_COLOR_FMT_RGB4444:
                    /* start color convert */
                    pStart = pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp;
                    pDst = (unsigned short *)(pRGBBuf + i * pVideoLogo->stride + j * 2);
                    r = *(pStart);
                    g = *(pStart + 1);
                    b = *(pStart + 2);
                    // fprintf(stderr, "Func: %s, line:%d, Bpp: %d, bmp stride: %d, Canvas stride: %d, h:%d, w:%d.\n",
                    //     __FUNCTION__, __LINE__, Bpp, stride, pVideoLogo->stride, i, j);
                    *pDst = convert_2bpp(r, g, b, s_OSDCompInfo[enFmt]);

                    break;

                case OSD_COLOR_FMT_RGB888:
                case OSD_COLOR_FMT_RGB8888:
                    memcpy(pRGBBuf + i * pVideoLogo->stride + j * 4, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);
                    *(pRGBBuf + i * pVideoLogo->stride + j * 4 + 3) = 0xff; /*alpha*/
                    break;

                default:
                    fprintf(stderr, "file(%s), line(%d), no such format!\n", __FILE__, __LINE__);
                    break;
                }
            }
            else if ((Bpp == 2) || (Bpp == 4)) /*..............*/
            {
                memcpy(pRGBBuf + i * pVideoLogo->stride + j * Bpp, pOrigBMPBuf + ((h - 1) - i) * stride + j * Bpp, Bpp);
            }
        }
    }

    free(pOrigBMPBuf);
    pOrigBMPBuf = NULL;

    fclose(pFile);
    return 0;
}

char *extract_extension(char *filename)
{
    char *pret = NULL;

    if (!filename)
    {
        fprintf(stderr, "filename can't be null!");
        return NULL;
    }

    unsigned int fnLen = strlen(filename);
    while (fnLen)
    {
        pret = filename + fnLen;
        if (*pret == '.')
            return (pret + 1);
        fnLen--;
    }

    return pret;
}

int load_image(const char *filename, OSD_LOGO_T *pVideoLogo)
{
    char *ext = extract_extension((char *)filename);

    if (ext && !strcmp(ext, "bmp"))
    {
        if (load_bitmap(filename, pVideoLogo))
        {
            fprintf(stderr, "load_bitmap error!\n");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "not supported image file!\n");
        return -1;
    }

    return 0;
}

int load_imageex(const char *filename, OSD_LOGO_T *pVideoLogo, OSD_COLOR_FMT_E enFmt)
{
    char *ext = extract_extension((char*)filename);

    if (ext && !strcmp(ext, "bmp"))
    {
        if (load_bitmapex(filename, pVideoLogo, enFmt))
        {
            fprintf(stderr, "load_bitmap error!\n");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "not supported image file!\n");
        return -1;
    }

    return 0;
}

int load_canvasex(const char *filename, OSD_LOGO_T *pVideoLogo, OSD_COLOR_FMT_E enFmt)
{
    char *ext = extract_extension((char *)filename);

    if (ext && !strcmp(ext, "bmp"))
    {
        if (load_bitmap_canvas(filename, pVideoLogo, enFmt))
        {
            fprintf(stderr, "load_bitmap error!\n");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "not supported image file!\n");
        return -1;
    }

    return 0;
}

int LoadBitMap2Surface(const char *pszFileName, const OSD_SURFACE_S *pstSurface, unsigned char *pu8Virt)
{
    OSD_LOGO_T stLogo;

    stLogo.stride = pstSurface->u16Stride;
    stLogo.pRGBBuffer = pu8Virt;

    return load_image(pszFileName, &stLogo);
}

int CreateSurfaceByBitMap(const char *pszFileName, OSD_SURFACE_S *pstSurface, unsigned char *pu8Virt)
{
    OSD_LOGO_T stLogo;

    stLogo.pRGBBuffer = pu8Virt;

    if (load_imageex(pszFileName, &stLogo, pstSurface->enColorFmt) < 0)
    {
        fprintf(stderr, "load bmp error!\n");
        return -1;
    }

    pstSurface->u16Height = stLogo.height;
    pstSurface->u16Width = stLogo.width;
    pstSurface->u16Stride = stLogo.stride;

    return 0;
}

int CreateSurfaceByCanvas(const char *pszFileName, OSD_SURFACE_S *pstSurface, unsigned char *pu8Virt, unsigned int u32Width, unsigned int u32Height, unsigned int u32Stride)
{
    OSD_LOGO_T stLogo;

    stLogo.pRGBBuffer = pu8Virt;
    stLogo.width = u32Width;
    stLogo.height = u32Height;
    stLogo.stride = u32Stride;

    if (load_canvasex(pszFileName, &stLogo, pstSurface->enColorFmt) < 0)
    {
        fprintf(stderr, "load bmp error!\n");
        return -1;
    }

    pstSurface->u16Height = u32Height;
    pstSurface->u16Width = u32Width;
    pstSurface->u16Stride = u32Stride;

    return 0;
}



// Function to calculate the squared difference between two colors in ARGB1555 format
uint32_t colorDistance8(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2) {
    return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

// Function to find the closest color in the palette
uint8_t findClosestPaletteIndex8(uint16_t color, MI_RGN_PaletteTable_t* paletteTable) {
    uint32_t minDistance = 65535;
    uint8_t bestIndex = 0;

    // Extract RGB components from ARGB1555 color
    uint8_t r = ((color >> 10) & 0x1F) << 3;  // Convert 5-bit to 8-bit
    uint8_t g = ((color >> 5) & 0x1F) << 3;   // Convert 5-bit to 8-bit
    uint8_t b = (color & 0x1F) << 3;          // Convert 5-bit to 8-bit
     
    for (uint8_t i = 1; i < 17; ++i) {// only 16 searched
        MI_RGN_PaletteElement_t* element = &paletteTable->astElement[i];
        uint32_t distance = colorDistance8(r, g, b, element->u8Red, element->u8Green, element->u8Blue);
        if (distance < minDistance) {
            minDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void convertBitmap1555ToI8(
    uint16_t* srcBitmap, uint32_t width, uint32_t height, 
    uint8_t* destBitmap, MI_RGN_PaletteTable_t* paletteTable)
{
    uint32_t numPixels = width * height;

    for (uint32_t i = 0; i < numPixels; ++i) {
        // Find the closest palette index for each ARGB1555 pixel
        uint8_t paletteIndex = findClosestPaletteIndex8(srcBitmap[i], paletteTable)+1;

        // Store the palette index in the I8 bitmap
        destBitmap[i] = paletteIndex;
    }
}

//static MI_RGN_PaletteTable_t g_stPaletteTable ;//= {{{0, 0, 0, 0}}};
MI_RGN_PaletteTable_t g_stPaletteTable =
{
    { //index0 ~ index15
        {255,   0,   0,   0}, // reserved
      
        {0xFF, 0xF8, 0x00, 0x00}, // 0x7C00 -> Red
        {0xFF, 0x00, 0xF8, 0x00}, // 0x03E0 -> Green
        {0xFF, 0x00, 0x00, 0xF8}, // 0x001F -> Blue
        {0xFF, 0xF8, 0xF8, 0x00}, // 0x7FE0 -> Yellow
        {0xFF, 0xF8, 0x00, 0xF8}, // 0x7C1F -> Magenta
        {0xFF, 0x00, 0xF8, 0xF8}, // 0x03FF -> Cyan
        {0xFF, 0xF8, 0xF8, 0xF8}, // 0x7FFF -> White
        {0x00, 0x00, 0x00, 0x00}, // 0x0000 -> Black
        {0xFF, 0x84, 0x10, 0x10}, // 0x4210 -> Gray (Darker)
        {0xFF, 0x42, 0x08, 0x08}, // 0x2108 -> Gray (Even Darker)
        {0xFF, 0x63, 0x18, 0xC6}, // 0x318C -> Gray (Medium)
        {0xFF, 0xAD, 0x52, 0xD6}, // 0x5AD6 -> Gray (Lighter)
        {0xFF, 0xCE, 0x73, 0x9C}, // 0x739C -> Gray (Light)
        {0xFF, 0x31, 0x8C, 0x6C}, // 0x18C6 -> Gray (Dark)
        {0xFF, 0x52, 0x52, 0x29}, // 0x2529 -> Gray (Medium Dark)
        {0xFF, 0xDE, 0x7B, 0xDE},  // 0x7BDE -> Gray (Lightest)
 
         //index17 ~ index31
         {  0,   0,   0,  30}, {  0,   0, 255,  60}, {  0, 128,   0,  90},
         {255,   0,   0, 120}, {  0, 255, 255, 150}, {255, 255,   0, 180}, {  0, 255,   0, 210},
         {255,   0, 255, 240}, {192, 192, 192, 255}, {128, 128, 128,  10}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index32 ~ index47
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index48 ~ index63
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index64 ~ index79
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index80 ~ index95
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index96 ~ index111
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index112 ~ index127
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index128 ~ index143
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index144 ~ index159
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index160 ~ index175
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index176 ~ index191
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index192 ~ index207
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index208 ~ index223
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index224 ~ index239
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         // (index236 :192,160,224 defalut colorkey)
         {192, 160, 224, 255}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         //index240 ~ index255
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0},
         {  0,   0,   0,   0}, {  0,   0,   0,   0}, {  0,   0,   0,   0}, {192, 160, 224, 255}
    }
};



void Convert1555ToRGBA(unsigned short* bitmap1555, unsigned char* rgbaData, unsigned int width, unsigned int height) {
    for (unsigned int i = 0; i < width * height; ++i) {
        unsigned short pixel1555 = bitmap1555[i];

        // Extract components
        unsigned char alpha = (pixel1555 & 0x8000) ? 255 : 0;  // 1-bit alpha, 0x8000 is the alpha bit mask
        unsigned char red = (pixel1555 & 0x7C00) >> 10;        // 5-bit red, shift right to align
        unsigned char green = (pixel1555 & 0x03E0) >> 5;       // 5-bit green
        unsigned char blue = (pixel1555 & 0x001F);             // 5-bit blue

        // Scale 5-bit colors to 8-bit
        red = (red << 3) | (red >> 2);     // Scale from 5-bit to 8-bit
        green = (green << 3) | (green >> 2); // Scale from 5-bit to 8-bit
        blue = (blue << 3) | (blue >> 2);   // Scale from 5-bit to 8-bit

        // Combine into RGBA
        rgbaData[i * 4 + 0] = red;
        rgbaData[i * 4 + 1] = green;
        rgbaData[i * 4 + 2] = blue;
        rgbaData[i * 4 + 3] = alpha;
    }
}


void copyRectARGB1555(
    uint16_t* srcBitmap, uint32_t srcWidth, uint32_t srcHeight,
    uint16_t* destBitmap, uint32_t destWidth, uint32_t destHeight,
    uint32_t srcX, uint32_t srcY, uint32_t width, uint32_t height,
    uint32_t destX, uint32_t destY)
{
    // Bounds checking
    if (srcX + width > srcWidth || srcY + height > srcHeight ||
        destX + width > destWidth || destY + height > destHeight){
        // Handle error: the rectangle is out of bounds
        printf("Error copyRectARGB1555 to %d : %d\r\n", destX, destY);
        return;
    }

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Calculate the source and destination indices
            uint32_t srcIndex = (srcY + y) * srcWidth + (srcX + x);
            uint32_t destIndex = (destY + y) * destWidth + (destX + x);

            //if (srcBitmap[srcIndex]==TRANSPARENT_COLOR)
             //   srcBitmap[srcIndex]=0x7FFF;
            // Copy the pixel
            destBitmap[destIndex] = srcBitmap[srcIndex];
        }
    }
}

/*
void copyRectI8Slow(
    uint8_t* srcBitmap, uint32_t srcWidth, uint32_t srcHeight,
    uint8_t* destBitmap, uint32_t destWidth, uint32_t destHeight,
    uint32_t srcX, uint32_t srcY, uint32_t width, uint32_t height,
    uint32_t destX, uint32_t destY)
{
    // Bounds checking
    if (srcX + width > srcWidth || srcY + height > srcHeight ||
        destX + width > destWidth || destY + height > destHeight){
        // Handle error: the rectangle is out of bounds
        printf("Error copyRectARGB1555 to %d : %d\r\n", destX, destY);
        return;
    }

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Calculate the source and destination indices
            uint32_t srcIndex = (srcY + y) * srcWidth + (srcX + x);
            uint32_t destIndex = (destY + y) * destWidth + (destX + x);

            //if (srcBitmap[srcIndex]==TRANSPARENT_COLOR)
             //   srcBitmap[srcIndex]=0x7FFF;
            // Copy the pixel
            destBitmap[destIndex] = srcBitmap[srcIndex];
        }
    }
}
*/
 
void copyRectI8(
    uint8_t* srcBitmap, uint32_t srcWidth, uint32_t srcHeight,
    uint8_t* destBitmap, uint32_t destWidth, uint32_t destHeight,
    uint32_t srcX, uint32_t srcY, uint32_t width, uint32_t height,
    uint32_t destX, uint32_t destY)
{
    // Bounds checking
    if (srcX + width > srcWidth || srcY + height > srcHeight ||
        destX + width > destWidth || destY + height > destHeight) {
        // Handle error: the rectangle is out of bounds
        printf("Error copyRectI8 to %d : %d\r\n", destX, destY);
        return;
    }

    // Calculate the starting indices once
    uint32_t srcStartIndex = srcY * srcWidth + srcX;
    uint32_t destStartIndex = destY * destWidth + destX;

    // Copy each row in one operation using memcpy
    for (uint32_t y = 0; y < height; ++y) {
        memcpy(&destBitmap[destStartIndex + y * destWidth],
               &srcBitmap[srcStartIndex + y * srcWidth],
               width);
    }
}



void ConvertI8ToRGBA(uint8_t* bitmapI8, uint8_t* rgbaData, uint32_t width, uint32_t height, MI_RGN_PaletteElement_t* palette) {
    for (uint32_t i = 0; i < width * height; ++i) {
        uint8_t index = bitmapI8[i];

        // Get RGBA color from the palette
        MI_RGN_PaletteElement_t color = palette[index];

        // Assign to RGBA data
        rgbaData[i * 4 + 0] = color.u8Red;
        rgbaData[i * 4 + 1] = color.u8Green;
        rgbaData[i * 4 + 2] = color.u8Blue;
        rgbaData[i * 4 + 3] = color.u8Alpha;
    }
}