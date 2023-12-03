#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <memory.h>
#include <math.h>

#include "jpeg_encoder.h"

namespace {
//-------------------------------------------------------------------------------
//标准亮度量化表 8*8，存储为一维形式
const unsigned char Luminance_Quantization_Table[64] = 
{
	16,  11,  10,  16,  24,  40,  51,  61,
	12,  12,  14,  19,  26,  58,  60,  55,
	14,  13,  16,  24,  40,  57,  69,  56,
	14,  17,  22,  29,  51,  87,  80,  62,
	18,  22,  37,  56,  68, 109, 103,  77,
	24,  35,  55,  64,  81, 104, 113,  92,
	49,  64,  78,  87, 103, 121, 120, 101,
	72,  92,  95,  98, 112, 100, 103,  99
};

//-------------------------------------------------------------------------------
//标准色差量化表 8*8，存储为一维形式
const unsigned char Chrominance_Quantization_Table[64] = 
{
	17,  18,  24,  47,  99,  99,  99,  99,
	18,  21,  26,  66,  99,  99,  99,  99,
	24,  26,  56,  99,  99,  99,  99,  99,
	47,  66,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99,
	99,  99,  99,  99,  99,  99,  99,  99
};

//-------------------------------------------------------------------------------
// ZigZag数组用于将DCT变换得到的系数矩阵重新排列成一个一维数组，注意到排列方式是按照低频到高频的顺序，即从左上角排到右下角。目的是为了更有效的进行后续的量化和熵编码
const char ZigZag[64] =
{ 
	0, 1, 5, 6,14,15,27,28,
	2, 4, 7,13,16,26,29,42,
	3, 8,12,17,25,30,41,43,
	9,11,18,24,31,40,44,53,
	10,19,23,32,39,45,52,54,
	20,22,33,38,46,51,55,60,
	21,34,37,47,50,56,59,61,
	35,36,48,49,57,58,62,63 
};     

//-------------------------------------------------------------------------------
// 编码是对量化后的一维数组，进行编码。
//编码流程是：1.先使用行程编码，按照0的个数，若一个数字y前有x个零，则编码为(X,Y)则一维数组的第一个值直流DC部分的值为(0,Y)。
//2.对Y进行编码，采用的是JPEG提供的一张标准码表。编码后会产生一个长度信息Z加上长度信息个的编码位
//3.将X和Z合并成一个十六进制形式M。
//4.此时再分别用到直流部分的编码表和交流部分的编码表（长度可能更大），分别对直流部分的M和交流部分的M进行编码。


//这里是亮度分量的DC部分的M的编码信息。第一个数组表示编码长度为数组索引号+1的被编码值的个数
const char Standard_DC_Luminance_NRCodes[] = { 0, 0, 7, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
//这个矩阵表示的是亮度分量的DC部分的M的值出现的频率由高到低排列，同时也对应着上面一个数组的编码长度递增的顺序。即此数组中的前7个元素用3个位进行编码，再其次的后面1个元素用4个位进行编码
const unsigned char Standard_DC_Luminance_Values[] = { 4, 5, 3, 2, 6, 1, 0, 7, 8, 9, 10, 11 };

//-------------------------------------------------------------------------------
//这两个数组于上面两个数组类似，只不过对应的是色度分量的DC部分
const char Standard_DC_Chrominance_NRCodes[] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
const unsigned char Standard_DC_Chrominance_Values[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

//-------------------------------------------------------------------------------
//这两个数组也与上面两个数组类似，只不过对应的是亮度分量的交流部分
const char Standard_AC_Luminance_NRCodes[] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
const unsigned char Standard_AC_Luminance_Values[] = 
{
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
	0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
	0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
	0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
	0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
	0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
	0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
	0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
	0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
	0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
	0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};

//-------------------------------------------------------------------------------
//这两个数组也与上面两个数组类似，只不过对应的是色度分量的交流部分
const char Standard_AC_Chrominance_NRCodes[] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
const unsigned char Standard_AC_Chrominance_Values[] =
{
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
	0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
	0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
	0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
	0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
	0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
	0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
	0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
	0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
	0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
	0xf9, 0xfa
};

}

//-------------------------------------------------------------------------------
JpegEncoder::JpegEncoder()
	: m_width(0)
	, m_height(0)
	, m_rgbBuffer(0)
{
	//初始化静态表格，准备好霍夫曼编码表用于后续的JPEG编码过程
	_initHuffmanTables();
}

//-------------------------------------------------------------------------------
JpegEncoder::~JpegEncoder()
{
	//结束时调用
	clean();
}

//-------------------------------------------------------------------------------
void JpegEncoder::clean(void)
{
	//清除m_rgbBuffer
	if(m_rgbBuffer) delete[] m_rgbBuffer;
	m_rgbBuffer=0;

	m_width=0;
	m_height=0;
}

//-------------------------------------------------------------------------------
// 将位图文件里的像素信息存在m_rgbBuffer中，存储顺序为图像的从上至下，从左至右
bool JpegEncoder::readFromBMP(const char* fileName)
{
	//开始时清理旧数据
	clean();

	//BMP 文件格式
	//BMP格式图片的文件头长度是54字节，包括14字节的文件数据头、40字节的位图信息数据头
#pragma pack(push, 2)
	typedef struct {
			unsigned short	bfType;//文件类型
			unsigned int	bfSize;//bmp文件的大小
			unsigned short	bfReserved1;//预留字段，通常为0
			unsigned short	bfReserved2;//预留字段，通常为0
			unsigned int	bfOffBits;//图片信息的开始位置
	} BITMAPFILEHEADER;

	typedef struct {
			unsigned int	biSize;//位图信息数据头的大小
			int				biWidth;//图像宽度
			int				biHeight;//图像高度
			unsigned short	biPlanes;//色彩平面的数量，必须为1
			unsigned short	biBitCount;//每像素用多少bit表示
			unsigned int	biCompression;//采用何种压缩方式，通常不压缩，即BI_RGB，对应值为0
			unsigned int	biSizeImage;//图片大小（原始位图数据的大小）
			int				biXPelsPerMeter;//横向分辨率（像素/米）
			int				biYPelsPerMeter;//纵向分辨率（像素/米）
			unsigned int	biClrUsed;//调色板中颜色数量，通常为0（不表示没有颜色）
			unsigned int	biClrImportant;//重要颜色的数量（通常被忽略），通常为0，表示每种颜色都重要
	} BITMAPINFOHEADER;
#pragma pack(pop)

	//打开文件
	FILE* fp = fopen(fileName, "rb");
	if(fp==0) return false;

	bool successed=false;
	do
	{
		BITMAPFILEHEADER fileHeader;//文件头
		BITMAPINFOHEADER infoHeader;//信息头
		//fread函数返回的是实际元素个数，1表示读取一个单位的的数据存入文件头fileHeader,即sizeof(fileHeader)
		if(1 != fread(&fileHeader, sizeof(fileHeader), 1, fp)) break;
		if(fileHeader.bfType!=0x4D42) break;//若读取的文件头信息中文件类型不为bmp

		if(1 != fread(&infoHeader, sizeof(infoHeader), 1, fp)) break;//读取信息头，则XXX
		if(infoHeader.biBitCount!=24 || infoHeader.biCompression!=0) break;//如果信息头中的位深度不为24和压缩方式不为0，则XXX
		int width = infoHeader.biWidth;//从信息头获取图像宽度
		int height = infoHeader.biHeight < 0 ? (-infoHeader.biHeight) : infoHeader.biHeight;//从信息头获取图像高度，若高度<0则置反。
		if((width&7)!=0 || (height&7)!=0) break;	//必须是8的倍数，7表示与二进制位0111与。而8的倍数最后3位必为000，即不能出现1-7

		int bmpSize = width*height*3;//buffer数组的长度

		unsigned char* buffer = new unsigned char[bmpSize];
		if(buffer==0) break;

		fseek(fp, fileHeader.bfOffBits, SEEK_SET);//将文件描述符指向的地址转向文件头中的图片信息开始位置
		//BMP存储像素值的方式为从下至上，从左至右，紧随着文件头存储的字节为图像最下一行的数值，从左下角开始依次存储。
		if(infoHeader.biHeight>0)//正向读取
		{
			for(int i=0; i<height; i++)
			{
				//文件最下面的为图像最上面的像素
				if(width != fread(buffer+(height-1-i)*width*3, 3, width, fp)) 
				{
					delete[] buffer; buffer=0;
					break;
				}
			}
		}
		else
		{
			//若高度值为-值，则可直接一次性读完所有像素数据
			if(width*height != fread(buffer, 3, width*height, fp))
			{
				delete[] buffer; buffer=0;
				break;
			}
		}

		m_rgbBuffer = buffer;
		m_width = width;
		m_height = height;
		successed=true;
	}while(false);

	fclose(fp);fp=0;
	
	return successed;
}

//-------------------------------------------------------------------------------
bool JpegEncoder::encodeToJPG(const char* fileName, int quality_scale)
{
	//尚未读取？
	if(m_rgbBuffer==0 || m_width==0 || m_height==0) return false;

	//输出文件
	FILE* fp = fopen(fileName, "wb");
	if(fp==0) return false;

	//初始化量化表
	_initQualityTables(quality_scale);

	//文件头
	_write_jpeg_header(fp);

	short prev_DC_Y = 0, prev_DC_Cb = 0, prev_DC_Cr = 0;
	int newByte=0, newBytePos=7;

	for(int yPos=0; yPos<m_height; yPos+=8)
	{
		for (int xPos=0; xPos<m_width; xPos+=8)
		{
			char yData[64], cbData[64], crData[64];
			short yQuant[64], cbQuant[64], crQuant[64];

			//转换颜色空间
			unsigned char* rgbBuffer = m_rgbBuffer + yPos * m_width * 3 + xPos * 3;
			_convertColorSpace(rgbBuffer, yData, cbData, crData);

			BitString outputBitString[128];
			int bitStringCounts;

			//Y通道压缩
			_foword_FDC(yData, yQuant, m_YTable);
			_doHuffmanEncoding(yQuant, prev_DC_Y, m_Y_DC_Huffman_Table, m_Y_AC_Huffman_Table, outputBitString, bitStringCounts); 
			_write_bitstring_(outputBitString, bitStringCounts, newByte, newBytePos, fp);

			//Cb通道压缩
			_foword_FDC(cbData, cbQuant, m_CbCrTable);			
			_doHuffmanEncoding(cbQuant, prev_DC_Cb, m_CbCr_DC_Huffman_Table, m_CbCr_AC_Huffman_Table, outputBitString, bitStringCounts);
			_write_bitstring_(outputBitString, bitStringCounts, newByte, newBytePos, fp);

			//Cr通道压缩
			_foword_FDC(crData, crQuant, m_CbCrTable);			
			_doHuffmanEncoding(crQuant, prev_DC_Cr, m_CbCr_DC_Huffman_Table, m_CbCr_AC_Huffman_Table, outputBitString, bitStringCounts);
			_write_bitstring_(outputBitString, bitStringCounts, newByte, newBytePos, fp);
		}
	}
	//flush remain data
	if(newBytePos!=7){
		_write_byte_(newByte,fp);
	}
	_write_word_(0xFFD9, fp); //Write End of Image Marker   
	
	fclose(fp);

	return true;
}

//-------------------------------------------------------------------------------
void JpegEncoder::_initHuffmanTables(void)
{
	memset(&m_Y_DC_Huffman_Table, 0, sizeof(m_Y_DC_Huffman_Table));
	_computeHuffmanTable(Standard_DC_Luminance_NRCodes, Standard_DC_Luminance_Values, m_Y_DC_Huffman_Table);

	memset(&m_Y_AC_Huffman_Table, 0, sizeof(m_Y_AC_Huffman_Table));
	_computeHuffmanTable(Standard_AC_Luminance_NRCodes, Standard_AC_Luminance_Values, m_Y_AC_Huffman_Table);

	memset(&m_CbCr_DC_Huffman_Table, 0, sizeof(m_CbCr_DC_Huffman_Table));
	_computeHuffmanTable(Standard_DC_Chrominance_NRCodes, Standard_DC_Chrominance_Values, m_CbCr_DC_Huffman_Table);

	memset(&m_CbCr_AC_Huffman_Table, 0, sizeof(m_CbCr_AC_Huffman_Table));
	_computeHuffmanTable(Standard_AC_Chrominance_NRCodes, Standard_AC_Chrominance_Values, m_CbCr_AC_Huffman_Table);
}
//-------------------------------------------------------------------------------
JpegEncoder::BitString JpegEncoder::_getBitCode(int value)
{
	BitString ret;
	int v = (value>0) ? value : -value;
	
	//bit 的长度
	int length = 0;
	for(length=0; v; v>>=1) length++;

	ret.value = value>0 ? value : ((1<<length)+value-1);
	ret.length = length;

	return ret;
};

//-------------------------------------------------------------------------------
void JpegEncoder::_initQualityTables(int quality_scale)
{
	if(quality_scale<=0) quality_scale=1;
	if(quality_scale>=100) quality_scale=99;
	//8*8的量化系数
	for(int i=0; i<64; i++)
	{	
		//将量化系数矩阵展开成一维数组（从左上->右下，锯齿形排列）
		int temp = ((int)(Luminance_Quantization_Table[i] * quality_scale + 50) / 100);
		if (temp<=0) temp = 1;
		if (temp>0xFF) temp = 0xFF;
		m_YTable[ZigZag[i]] = (unsigned char)temp;
		temp = ((int)(Chrominance_Quantization_Table[i] * quality_scale + 50) / 100);
		if (temp<=0) 	temp = 1;
		if (temp>0xFF) temp = 0xFF;
		m_CbCrTable[ZigZag[i]] = (unsigned char)temp;
	}
}

//-------------------------------------------------------------------------------
void JpegEncoder::_computeHuffmanTable(const char* nr_codes, const unsigned char* std_table, BitString* huffman_table)
{
	unsigned char pos_in_table = 0;
	unsigned short code_value = 0;

	for(int k = 1; k <= 16; k++)
	{
		for(int j = 1; j <= nr_codes[k-1]; j++)
		{
			huffman_table[std_table[pos_in_table]].value = code_value;
			huffman_table[std_table[pos_in_table]].length = k;
			pos_in_table++;
			code_value++;
		}
		code_value <<= 1;
	}  
}

//-------------------------------------------------------------------------------
void JpegEncoder::_write_byte_(unsigned char value, FILE* fp)
{
	_write_(&value, 1, fp);
}

//-------------------------------------------------------------------------------
void JpegEncoder::_write_word_(unsigned short value, FILE* fp)
{
	unsigned short _value = ((value>>8)&0xFF) | ((value&0xFF)<<8);
	_write_(&_value, 2, fp);
}

//-------------------------------------------------------------------------------
void JpegEncoder::_write_(const void* p, int byteSize, FILE* fp)
{
	fwrite(p, 1, byteSize, fp);
}

//-------------------------------------------------------------------------------
void JpegEncoder::_doHuffmanEncoding(const short* DU, short& prevDC, const BitString* HTDC, const BitString* HTAC, 
	BitString* outputBitString, int& bitStringCounts)
{
	BitString EOB = HTAC[0x00];
	BitString SIXTEEN_ZEROS = HTAC[0xF0];

	int index=0;

	// encode DC
	int dcDiff = (int)(DU[0] - prevDC);
	prevDC = DU[0];

	if (dcDiff == 0) 
		outputBitString[index++] = HTDC[0];
	else
	{
		BitString bs = _getBitCode(dcDiff);

		outputBitString[index++] = HTDC[bs.length];
		outputBitString[index++] = bs;
	}

	// encode ACs
	int endPos=63; //end0pos = first element in reverse order != 0
	while((endPos > 0) && (DU[endPos] == 0)) endPos--;

	for(int i=1; i<=endPos; )
	{
		int startPos = i;
		while((DU[i] == 0) && (i <= endPos)) i++;

		int zeroCounts = i - startPos;
		if (zeroCounts >= 16)
		{
			for (int j=1; j<=zeroCounts/16; j++)
				outputBitString[index++] = SIXTEEN_ZEROS;
			zeroCounts = zeroCounts%16;
		}

		BitString bs = _getBitCode(DU[i]);

		outputBitString[index++] = HTAC[(zeroCounts << 4) | bs.length];
		outputBitString[index++] = bs;
		i++;
	}

	if (endPos != 63)
		outputBitString[index++] = EOB;

	bitStringCounts = index;
}

//-------------------------------------------------------------------------------
void JpegEncoder::_write_bitstring_(const BitString* bs, int counts, int& newByte, int& newBytePos, FILE* fp)
{
	unsigned short mask[] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768};
	
	for(int i=0; i<counts; i++)
	{
		int value = bs[i].value;
		int posval = bs[i].length - 1;

		while (posval >= 0)
		{
			if ((value & mask[posval]) != 0)
			{
				newByte = newByte  | mask[newBytePos];
			}
			posval--;
			newBytePos--;
			if (newBytePos < 0)
			{
				// Write to stream
				_write_byte_((unsigned char)(newByte), fp);
				if (newByte == 0xFF)
				{
					// Handle special case
					_write_byte_((unsigned char)(0x00), fp);
				}

				// Reinitialize
				newBytePos = 7;
				newByte = 0;
			}
		}
	}
}

//-------------------------------------------------------------------------------
void JpegEncoder::_convertColorSpace(const unsigned char* rgbBuffer, char* yData, char* cbData, char* crData)
{
	for (int y=0; y<8; y++)
	{
		const unsigned char* p = rgbBuffer + y*m_width*3;
		for (int x=0; x<8; x++)
		{
			unsigned char B = *p++;
			unsigned char G = *p++;
			unsigned char R = *p++;

			yData[y*8+x] = (char)(0.299f * R + 0.587f * G + 0.114f * B - 128);
			cbData[y*8+x] = (char)(-0.1687f * R - 0.3313f * G + 0.5f * B );
			crData[y*8+x] = (char)(0.5f * R - 0.4187f * G - 0.0813f * B);
		}
	}
}

//-------------------------------------------------------------------------------
void JpegEncoder::_foword_FDC(const char* channel_data, short* fdc_data, const unsigned char* quant_table)
{
	const float PI = 3.1415926f;
	for(int v=0; v<8; v++)
	{
		for(int u=0; u<8; u++)
		{
			float alpha_u = (u==0) ? 1.f/sqrt(8.0f) : 0.5f;
			float alpha_v = (v==0) ? 1.f/sqrt(8.0f) : 0.5f;

			float temp = 0.f;
			for(int x=0; x<8; x++)
			{
				for(int y=0; y<8; y++)
				{
					float data = channel_data[y*8+x];

					data *= cosf((2*x+1)*u*PI/16.0f);
					data *= cosf((2*y+1)*v*PI/16.0f);

					temp += data;
				}
			}
			int zigZagIndex = ZigZag[v * 8 + u];

			//量化
			temp *= alpha_u*alpha_v/ quant_table[zigZagIndex];
			fdc_data[zigZagIndex] = (short) ((short)(temp + 16384.5) - 16384);
		}
	}
}

//-------------------------------------------------------------------------------
void JpegEncoder::_write_jpeg_header(FILE* fp)
{
	//SOI
	_write_word_(0xFFD8, fp);		// marker = 0xFFD8

	//APPO
	_write_word_(0xFFE0,fp);		// marker = 0xFFE0
	_write_word_(16, fp);			// length = 16 for usual JPEG, no thumbnail
	_write_("JFIF", 5, fp);			// 'JFIF\0'
	_write_byte_(1, fp);			// version_hi
	_write_byte_(1, fp);			// version_low
	_write_byte_(0, fp);			// xyunits = 0 no units, normal density
	_write_word_(1, fp);			// xdensity
	_write_word_(1, fp);			// ydensity
	_write_byte_(0, fp);			// thumbWidth
	_write_byte_(0, fp);			// thumbHeight

	//DQT
	_write_word_(0xFFDB, fp);		//marker = 0xFFDB
	_write_word_(132, fp);			//size=132
	_write_byte_(0, fp);			//QTYinfo== 0:  bit 0..3: number of QT = 0 (table for Y) 
									//				bit 4..7: precision of QT
									//				bit 8	: 0
	_write_(m_YTable, 64, fp);		//YTable
	_write_byte_(1, fp);			//QTCbinfo = 1 (quantization table for Cb,Cr)
	_write_(m_CbCrTable, 64, fp);	//CbCrTable

	//SOFO
	_write_word_(0xFFC0, fp);			//marker = 0xFFC0
	_write_word_(17, fp);				//length = 17 for a truecolor YCbCr JPG
	_write_byte_(8, fp);				//precision = 8: 8 bits/sample 
	_write_word_(m_height&0xFFFF, fp);	//height
	_write_word_(m_width&0xFFFF, fp);	//width
	_write_byte_(3, fp);				//nrofcomponents = 3: We encode a truecolor JPG

	_write_byte_(1, fp);				//IdY = 1
	_write_byte_(0x11, fp);				//HVY sampling factors for Y (bit 0-3 vert., 4-7 hor.)(SubSamp 1x1)
	_write_byte_(0, fp);				//QTY  Quantization Table number for Y = 0

	_write_byte_(2, fp);				//IdCb = 2
	_write_byte_(0x11, fp);				//HVCb = 0x11(SubSamp 1x1)
	_write_byte_(1, fp);				//QTCb = 1

	_write_byte_(3, fp);				//IdCr = 3
	_write_byte_(0x11, fp);				//HVCr = 0x11 (SubSamp 1x1)
	_write_byte_(1, fp);				//QTCr Normally equal to QTCb = 1
	
	//DHT
	_write_word_(0xFFC4, fp);		//marker = 0xFFC4
	_write_word_(0x01A2, fp);		//length = 0x01A2
	_write_byte_(0, fp);			//HTYDCinfo bit 0..3	: number of HT (0..3), for Y =0
									//			bit 4		: type of HT, 0 = DC table,1 = AC table
									//			bit 5..7	: not used, must be 0
	_write_(Standard_DC_Luminance_NRCodes, sizeof(Standard_DC_Luminance_NRCodes), fp);	//DC_L_NRC
	_write_(Standard_DC_Luminance_Values, sizeof(Standard_DC_Luminance_Values), fp);		//DC_L_VALUE
	_write_byte_(0x10, fp);			//HTYACinfo
	_write_(Standard_AC_Luminance_NRCodes, sizeof(Standard_AC_Luminance_NRCodes), fp);
	_write_(Standard_AC_Luminance_Values, sizeof(Standard_AC_Luminance_Values), fp); //we'll use the standard Huffman tables
	_write_byte_(0x01, fp);			//HTCbDCinfo
	_write_(Standard_DC_Chrominance_NRCodes, sizeof(Standard_DC_Chrominance_NRCodes), fp);
	_write_(Standard_DC_Chrominance_Values, sizeof(Standard_DC_Chrominance_Values), fp);
	_write_byte_(0x11, fp);			//HTCbACinfo
	_write_(Standard_AC_Chrominance_NRCodes, sizeof(Standard_AC_Chrominance_NRCodes), fp);
	_write_(Standard_AC_Chrominance_Values, sizeof(Standard_AC_Chrominance_Values), fp);

	//SOS
	_write_word_(0xFFDA, fp);		//marker = 0xFFC4
	_write_word_(12, fp);			//length = 12
	_write_byte_(3, fp);			//nrofcomponents, Should be 3: truecolor JPG

	_write_byte_(1, fp);			//Idy=1
	_write_byte_(0, fp);			//HTY	bits 0..3: AC table (0..3)
									//		bits 4..7: DC table (0..3)
	_write_byte_(2, fp);			//IdCb
	_write_byte_(0x11, fp);			//HTCb

	_write_byte_(3, fp);			//IdCr
	_write_byte_(0x11, fp);			//HTCr

	_write_byte_(0, fp);			//Ss not interesting, they should be 0,63,0
	_write_byte_(0x3F, fp);			//Se
	_write_byte_(0, fp);			//Bf
}
