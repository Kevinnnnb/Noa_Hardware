#define DISPLAY_WIDTH  tft.width()
#define DISPLAY_HEIGHT tft.height()
#define BUFFER_SIZE 320            // Optimum is >= GIF width or integral division of width
// #define USE_DMA
#ifdef USE_DMA
  uint16_t usTemp[2][BUFFER_SIZE]; // Global to support DMA use
#else
  uint16_t usTemp[1][BUFFER_SIZE];    // Global to support DMA use
#endif
bool     dmaBuf = 0;

// Function to increase brightness of a color
uint16_t increaseBrightness(uint16_t color) {
  // Extract RGB components
  uint16_t red = (color & 0xF800) >> 11;
  uint16_t green = (color & 0x07E0) >> 5;
  uint16_t blue = color & 0x001F;

  // Increase brightness
  red = min(red + (31 - red) / 2, 31);
  green = min(green + (63 - green) / 2, 63);
  blue = min(blue + (31 - blue) / 2, 31);

  // Combine back to RGB565
  return (red << 11) | (green << 5) | blue;
}

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  // Display bounds check and cropping
  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  
  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  // Old image disposal
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < iWidth)
    {
      c = ucTransparent - 1;
      d = &usTemp[0][0];
      while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE )
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          uint16_t color = usPalette[c];
          if (color == 0xFFFF) { // Pixel is white
            *d++ = color;
          } else {
            *d++ = increaseBrightness(color);
          }
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        // DMA would degrade performance here due to short line segments
        
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.pushPixels(usTemp, iCount);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  }
  else
  {
    s = pDraw->pPixels;

    // Unroll the first pass to boost DMA performance
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    if (iWidth <= BUFFER_SIZE)
      for (iCount = 0; iCount < iWidth; iCount++) {
        uint16_t color = usPalette[*s++];
        if (color == 0xFFFF) { // Pixel is white
          usTemp[dmaBuf][iCount] = color;
        } else {
          usTemp[dmaBuf][iCount] = increaseBrightness(color);
        }
      }
    else
      for (iCount = 0; iCount < BUFFER_SIZE; iCount++) {
        uint16_t color = usPalette[*s++];
        if (color == 0xFFFF) { // Pixel is white
          usTemp[dmaBuf][iCount] = color;
        } else {
          usTemp[dmaBuf][iCount] = increaseBrightness(color);
        }
      }

#ifdef USE_DMA // 71.6 fps (ST7796 84.5 fps)
    tft.dmaWait();
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
    dmaBuf = !dmaBuf;
#else // 57.0 fps
    int yShift = 480/2 - pDraw->iHeight/2;
    int xShift = 320/2 - pDraw->iWidth/2;
    tft.setAddrWindow(pDraw->iX+xShift, y+yShift, iWidth, 1);
    tft.pushPixels(&usTemp[0][0], iCount);
#endif

    iWidth -= iCount;
    // Loop if pixel buffer smaller than width
    while (iWidth > 0)
    {
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      if (iWidth <= BUFFER_SIZE)
        for (iCount = 0; iCount < iWidth; iCount++) {
          uint16_t color = usPalette[*s++];
          if (color == 0xFFFF) { // Pixel is white
            usTemp[dmaBuf][iCount] = color;
          } else {
            usTemp[dmaBuf][iCount] = increaseBrightness(color);
          }
        }
      else
        for (iCount = 0; iCount < BUFFER_SIZE; iCount++) {
          uint16_t color = usPalette[*s++];
          if (color == 0xFFFF) { // Pixel is white
            usTemp[dmaBuf][iCount] = color;
          } else {
            usTemp[dmaBuf][iCount] = increaseBrightness(color);
          }
        }

#ifdef USE_DMA
      tft.dmaWait();
      tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
      dmaBuf = !dmaBuf;
#else
      tft.pushPixels(&usTemp[0][0], iCount);
#endif
      iWidth -= iCount;
    }
  }
} /* GIFDraw() */
