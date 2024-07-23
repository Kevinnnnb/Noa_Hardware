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

// Function to reduce red and increase green in a color
uint16_t adjustColor(uint16_t color) {
  // Extraire les composants RGB
  uint16_t red = (color >> 11) & 0x1F;
  uint16_t green = (color >> 5) & 0x3F;
  uint16_t blue = color & 0x1F;

  // Ajuster les valeurs de couleur pour augmenter le rouge et réduire le vert
  red = max(green -10, 0);      // Augmenter significativement le rouge
  green = min(blue + 15, 31);     // Réduire considérablement le vert
  blue = red;                    // Laisser le bleu inchangé

  // Combiner les composants ajustés en RGB565
  return (red << 11) | (green << 5) | blue;
}





// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  // Limitation et découpage des dimensions d'affichage
  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // ligne actuelle
  
  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  // Gestion de la transparence de l'image ancienne
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restaurer la couleur de fond
  {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Appliquer les nouveaux pixels à l'image principale
  if (pDraw->ucHasTransparency) // si la transparence est utilisée
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // compter les pixels non transparents
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = &usTemp[0][0];
      while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE) {
        c = *s++;
        if (c == ucTransparent) // terminé, s'arrêter
        {
          s--; // revenir en arrière pour le traiter comme transparent
        } else // opaque
        {
          uint16_t color = usPalette[c];
          if (color == 0xFFFF || color == 0x0000) { // Pixel est blanc ou noir
            *d++ = color;
          } else {
            *d++ = adjustColor(color);
          }
          iCount++;
        }
      }
      if (iCount) // des pixels opaques ?
      {
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.pushPixels(usTemp, iCount);
        x += iCount;
        iCount = 0;
      }
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  } else {
    s = pDraw->pPixels;

    if (iWidth <= BUFFER_SIZE)
      for (iCount = 0; iCount < iWidth; iCount++) {
        uint16_t color = usPalette[*s++];
        if (color == 0xFFFF || color == 0x0000) {
          usTemp[dmaBuf][iCount] = color;
        } else {
          usTemp[dmaBuf][iCount] = adjustColor(color);
        }
      }
    else
      for (iCount = 0; iCount < BUFFER_SIZE; iCount++) {
        uint16_t color = usPalette[*s++];
        if (color == 0xFFFF || color == 0x0000) {
          usTemp[dmaBuf][iCount] = color;
        } else {
          usTemp[dmaBuf][iCount] = adjustColor(color);
        }
      }

#ifdef USE_DMA
    tft.dmaWait();
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
    dmaBuf = !dmaBuf;
#else
    int yShift = 480/2 - pDraw->iHeight/2;
    int xShift = 320/2 - pDraw->iWidth/2;
    tft.setAddrWindow(pDraw->iX+xShift, y+yShift, iWidth, 1);
    tft.pushPixels(&usTemp[0][0], iCount);
#endif

    iWidth -= iCount;
    while (iWidth > 0) {
      if (iWidth <= BUFFER_SIZE)
        for (iCount = 0; iCount < iWidth; iCount++) {
          uint16_t color = usPalette[*s++];
          if (color == 0xFFFF || color == 0x0000) {
            usTemp[dmaBuf][iCount] = color;
          } else {
            usTemp[dmaBuf][iCount] = adjustColor(color);
          }
        }
      else
        for (iCount = 0; iCount < BUFFER_SIZE; iCount++) {
          uint16_t color = usPalette[*s++];
          if (color == 0xFFFF || color == 0x0000) {
            usTemp[dmaBuf][iCount] = color;
          } else {
            usTemp[dmaBuf][iCount] = adjustColor(color);
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
}
