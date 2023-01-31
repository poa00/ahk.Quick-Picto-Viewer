
static void Jpeg2PDF_SetXREF(PJPEG2PDF pPDF, int index, int offset, char c) {
  if(INDEX_USE_PPDF == index) index = pPDF->pdfObj;

  if('f' == c) 
    sprintf((char *)pPDF->pdfXREF[index], "%010d 65535 f\r\n", offset);
  else
    sprintf((char *)pPDF->pdfXREF[index], "%010d 00000 %c\r\n", offset, c);

  // if (JPEG2PDF_DEBUG)
  //    printf("pPDF->pdfXREF[%d] = %s", index, (int)pPDF->pdfXREF[index], 3,4,5,6);
}

PJPEG2PDF Jpeg2PDF_BeginDocument(double pdfW, double pdfH, int pdf_dpi) {
// based on https://www.codeproject.com/Articles/29879/Simplest-PDF-Generating-API-for-JPEG-Image-Content

  PJPEG2PDF pPDF;
  pPDF = (PJPEG2PDF)malloc(sizeof(JPEG2PDF));
  if (pPDF)
  {
    memset(pPDF, 0, sizeof(JPEG2PDF));
    // if (JPEG2PDF_DEBUG)
    //    printf("PDF List Inited (pPDF = %p)\n", (int)pPDF, 2,3,4,5,6);
    pPDF->pageW = (UINT32)(pdfW * pdf_dpi);
    pPDF->pageH = (UINT32)(pdfH * pdf_dpi);
    // fnOutputDebug("PDF Page Size: " + std::to_string(pPDF->pageW) + " x " + std::to_string(pPDF->pageH) + " @ " + std::to_string(pdf_dpi) + " dpi");

    pPDF->currentOffSet = 0;
    Jpeg2PDF_SetXREF(pPDF, 0, pPDF->currentOffSet, 'f');
    pPDF->currentOffSet += sprintf((char *)pPDF->pdfHeader, "%%PDF-1.3\r\n%%%cPDF generated by Quick Picture Viewer v5.%c\r\n", 0xFF, 0xFF);
    // if (JPEG2PDF_DEBUG)
    //    printf("[pPDF=%p], header: %s", (int)pPDF, (int)pPDF->pdfHeader,3,4,5,6);
    
    pPDF->imgObj = 0;
    pPDF->pdfObj = 2;    // 0 & 1 was reserved for xref & document Root
  }
  
  return pPDF;
}

STATUS Jpeg2PDF_AddJpeg(PJPEG2PDF pPDF, UINT32 imgW, UINT32 imgH, UINT32 fileSize, UINT8 *pJpeg, UINT8 isColor) {
  STATUS result = ERROR;
  PJPEG2PDF_NODE pNode;

  if (pPDF)
  {
    if (pPDF->nodeCount >= MAX_PDF_PAGES)
    {
       // fnOutputDebug("Add JPEG into PDF. Image skipped. The maximum number of pages has been reached for this PDF file. MAX=" + std::to_string(MAX_PDF_PAGES));
       return result;
    }

    pNode = (PJPEG2PDF_NODE)malloc(sizeof(JPEG2PDF_NODE));
    if (pNode)
    {
      UINT32 nChars, currentImageObject;
      UINT8 *pFormat, lenStr[256];
      pNode->JpegW = imgW;
      pNode->JpegH = imgH;
      pNode->JpegSize = fileSize;
      pNode->pJpeg = (UINT8 *)malloc(pNode->JpegSize);
      pNode->pNext = NULL;
    
      if (pNode->pJpeg != NULL)
      {
        memcpy(pNode->pJpeg, pJpeg, pNode->JpegSize);
        
        // Image Object
        Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
        currentImageObject = pPDF->pdfObj;

        pPDF->currentOffSet += sprintf((char *)pNode->preFormat, "\r\n%d 0 obj\r\n<</Type/XObject/Subtype/Image/Filter/DCTDecode/BitsPerComponent 8/ColorSpace/%s/Width %d/Height %d/Length %d>>\r\nstream\r\n",
          pPDF->pdfObj, ((isColor)? "DeviceRGB" : "DeviceGray"), pNode->JpegW, pNode->JpegH, pNode->JpegSize);
        
        pPDF->currentOffSet += pNode->JpegSize;
        pFormat = pNode->pstFormat;
        nChars = sprintf((char *)pFormat, "\r\nendstream\r\nendobj\r\n");
        pPDF->currentOffSet += nChars;  pFormat += nChars;
        pPDF->pdfObj++;

        // Page Object
        Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
        pNode->PageObj = pPDF->pdfObj;
        nChars = sprintf((char *)pFormat, "%d 0 obj\r\n<</Type/Page/Parent 1 0 R/MediaBox[0 0 %d %d]/Contents %d 0 R/Resources %d 0 R>>\r\nendobj\r\n",
            pPDF->pdfObj, pPDF->pageW, pPDF->pageH, pPDF->pdfObj+1, pPDF->pdfObj + 3);
        pPDF->currentOffSet += nChars;  pFormat += nChars;
        pPDF->pdfObj++;

        // Contents Object in Page Object
        Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
        sprintf((char *)lenStr, "q\r\n1 0 0 1 %.2f %.2f cm\r\n%.2f 0 0 %.2f 0 0 cm\r\n/I%d Do\r\nQ\r\n",
            PDF_LEFT_MARGIN, PDF_TOP_MARGIN, pPDF->pageW - 2*PDF_LEFT_MARGIN, pPDF->pageH - 2*PDF_TOP_MARGIN, pPDF->imgObj);
        nChars = sprintf((char *)pFormat, "%d 0 obj\r\n<</Length %d 0 R>>stream\r\n%sendstream\r\nendobj\r\n",
            pPDF->pdfObj, pPDF->pdfObj+1, lenStr);
        pPDF->currentOffSet += nChars;  pFormat += nChars;
        pPDF->pdfObj++;

        // Length Object in Contents Object
        Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
        nChars = sprintf((char *)pFormat, "%d 0 obj\r\n%ld\r\nendobj\r\n", pPDF->pdfObj, strlen((char *)lenStr));
        pPDF->currentOffSet += nChars;  pFormat += nChars;
        pPDF->pdfObj++;
        
        // Resources Object in Page Object
        Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
        nChars = sprintf((char *)pFormat, "%d 0 obj\r\n<</ProcSet[/PDF/%s]/XObject<</I%d %d 0 R>>>>\r\nendobj\r\n",
                pPDF->pdfObj, ((isColor)? "ImageC" : "ImageB"), pPDF->imgObj, currentImageObject);
        pPDF->currentOffSet += nChars;  pFormat += nChars;
        pPDF->pdfObj++;
        pPDF->imgObj++;

        // Update the Link List
        pPDF->nodeCount++;
        if(1 == pPDF->nodeCount) {
          pPDF->pFirstNode = pNode;
        } else {
          pPDF->pLastNode->pNext = pNode;
        }

        pPDF->pLastNode = pNode;
        result = IDOK;
      } // pNode->pJpeg allocated OK
    } // pNode is valid
  } // pPDF is valid

  return result;
}

UINT32 Jpeg2PDF_EndDocument(PJPEG2PDF pPDF) {
  UINT32 headerSize, tailerSize, pdfSize = 0;
  
  if (pPDF)
  {
    UINT8 strKids[MAX_PDF_PAGES * MAX_KIDS_STRLEN], *pTail = pPDF->pdfTailer;
    UINT32 i, nChars, xrefOffSet;
    PJPEG2PDF_NODE pNode;
    
    // Catalog Object. This is the Last Object
    Jpeg2PDF_SetXREF(pPDF, INDEX_USE_PPDF, pPDF->currentOffSet, 'n');
    nChars = sprintf((char *)pTail, "%d 0 obj\r\n<</Type/Catalog/Pages 1 0 R>>\r\nendobj\r\n", pPDF->pdfObj);
    pPDF->currentOffSet += nChars;  pTail += nChars;
    
    // Pages Object. It's always the Object 1
    Jpeg2PDF_SetXREF(pPDF, 1, pPDF->currentOffSet, 'n');

    strKids[0] = 0;
    pNode = pPDF->pFirstNode;
    while (pNode != NULL)
    {
      UINT8 curStr[9];
      sprintf((char *)curStr, "%d 0 R ", pNode->PageObj);
      strcat((char *)strKids, (char *)curStr);
      pNode = pNode->pNext;
    }

    if (strlen((char *)strKids) > 1 && strKids[strlen((char *)strKids) - 1] == ' ') strKids[strlen((char *)strKids) - 1] = 0;

    nChars = sprintf((char *)pTail, "1 0 obj\r\n<</Type /Pages /Kids [%s] /Count %d>>\r\nendobj\r\n", strKids, pPDF->nodeCount);
    pPDF->currentOffSet += nChars;  pTail += nChars;

    // The xref & the rest of the tail
    xrefOffSet = pPDF->currentOffSet;
    nChars = sprintf((char *)pTail, "xref\r\n0 %d\r\n", pPDF->pdfObj+1);
    pPDF->currentOffSet += nChars;  pTail += nChars;
    
    for (i=0; i<=pPDF->pdfObj; i++)
    {
       nChars = sprintf((char *)pTail, "%s", pPDF->pdfXREF[i]);
       pPDF->currentOffSet += nChars;  pTail += nChars;
    }
    
    nChars = sprintf((char *)pTail, "trailer\r\n<</Root %d 0 R /Size %d>>\r\n", pPDF->pdfObj, pPDF->pdfObj+1);
    pPDF->currentOffSet += nChars;  pTail += nChars;
    
    nChars = sprintf((char *)pTail, "startxref\r\n%d\r\n%%%%EOF\r\n", xrefOffSet);
    pPDF->currentOffSet += nChars;  pTail += nChars;
  }
  
  headerSize = (UINT32)strlen((char *)pPDF->pdfHeader);
  tailerSize = (UINT32)strlen((char *)pPDF->pdfTailer);
  if ( headerSize && tailerSize && ( pPDF->currentOffSet > headerSize + tailerSize ) ) {
     pdfSize = pPDF->currentOffSet;
  }
  
  return pdfSize;
}

STATUS Jpeg2PDF_GetFinalDocumentAndCleanup(PJPEG2PDF pPDF, UINT8 *outPDF, UINT32 *outPDFSize, UINT32 tehPDFSize) {
// based on https://www.codeproject.com/Articles/29879/Simplest-PDF-Generating-API-for-JPEG-Image-Content

  STATUS result = ERROR;
  if (pPDF)
  {
    PJPEG2PDF_NODE pNode, pFreeCurrent;
    if (outPDF && (tehPDFSize >= pPDF->currentOffSet))
    {
      UINT32 nBytes, nBytesOut = 0;
      UINT8 *pOut = outPDF;
      
      nBytes = (UINT32)strlen((char *)pPDF->pdfHeader);
      memcpy(pOut, pPDF->pdfHeader, nBytes);
      nBytesOut += nBytes; pOut += nBytes;
      
      pNode = pPDF->pFirstNode;
      while (pNode != NULL)
      {
        nBytes = (UINT32)strlen((char *)pNode->preFormat);
        memcpy(pOut, pNode->preFormat, nBytes);
        nBytesOut += nBytes; pOut += nBytes;
        
        nBytes = pNode->JpegSize;
        memcpy(pOut, pNode->pJpeg, nBytes);
        nBytesOut += nBytes; pOut += nBytes;
        
        nBytes = (UINT32)strlen((char *)pNode->pstFormat);
        memcpy(pOut, pNode->pstFormat, nBytes);
        nBytesOut += nBytes; pOut += nBytes;
        
        pNode = pNode->pNext;
      }
      
      nBytes = (UINT32)strlen((char *)pPDF->pdfTailer);
      memcpy(pOut, pPDF->pdfTailer, nBytes);
      nBytesOut += nBytes; pOut += nBytes;
      
      *outPDFSize = nBytesOut;
      result = (nBytesOut>950) ? IDOK : ERROR;
      // fnOutputDebug("Jpeg2PDF_GetFinalDocumentAndCleanup(): " + std::to_string(nBytesOut));
    }
    
    pNode = pPDF->pFirstNode;
    while (pNode != NULL)
    {
      if (pNode->pJpeg)
         free(pNode->pJpeg);

      pFreeCurrent = pNode;
      pNode = pNode->pNext;
      free(pFreeCurrent);
    }
    
    if (pPDF)
    {
       free(pPDF);
       pPDF = NULL;
    }
  }
  
  return result;
}

static int get_jpeg_size(unsigned char* data, unsigned int data_size, unsigned short *width, unsigned short *height) {
  // Check for valid JPEG image
  int i = 0;   // Keeps track of the position within the file
  if (data[i] == 0xFF && data[i+1] == 0xD8 && data[i+2] == 0xFF && data[i+3] == 0xE0)
  {
    i += 4;
    // Check for valid JPEG header (null terminated JFIF)
    if (data[i+2] == 'J' && data[i+3] == 'F' && data[i+4] == 'I' && data[i+5] == 'F' && data[i+6] == 0x00)
    {
      // Retrieve the block length of the first block since the first block will not contain the size of file
      unsigned short block_length = data[i] * 256 + data[i+1];
      while (i<(int)data_size)
      {
        i+=block_length;                    // Increase the file index to get to the next block
        if(i >= (int)data_size) return 0;   // Check to protect against segmentation faults
        if(data[i] != 0xFF) return 0;       // Check that we are truly at the start of another block
        if(data[i+1] == 0xC0) {             // 0xFFC0 is the "Start of frame" marker which contains the file size
          // The structure of the 0xFFC0 block is quite simple [0xFFC0][ushort length][uchar precision][ushort x][ushort y]
          *height = data[i+5]*256 + data[i+6];
          *width = data[i+7]*256 + data[i+8];
          return 1;
        } else
        {
          i+=2;                                       // Skip the block marker
          block_length = data[i] * 256 + data[i+1];   // Go to the next block
        }
      }
      return 0;                     // If this point is reached then no size was found
    } else { return 0; }            // Not a valid JFIF string
  } else { return 0; }              // Not a valid SOI header
}

