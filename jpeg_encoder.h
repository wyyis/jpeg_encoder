#ifndef __JPEG_ENCODER_HEADER__
#define __JPEG_ENCODER_HEADER__

// 将BMP格式的图像转化为JPEG格式
class JpegEncoder
{


	//存储供调用的方法
public:
	/** 清理数据 */
	void clean(void);

	/** 从BMP文件中读取文件，仅支持24bit，图像的尺寸长度必须是8的倍数的文件 */
	bool readFromBMP(const char* fileName);

	/** 压缩到jpg文件中，quality_scale表示质量，取值范围(0,100), 数字越大压缩比例越高*/
	bool encodeToJPG(const char* fileName, int quality_scale);

private:
	//定义私有变量
	//图像宽高像素数
	int				m_width;
	int				m_height;
	//存储从BMP文件读取的RGB格式的数据
	unsigned char*	m_rgbBuffer;
	//存储标准亮度分量的量化表
	unsigned char	m_YTable[64];
	//存储标准色差的量化表
	unsigned char	m_CbCrTable[64];
	// 存储表示霍夫曼编码的codeword。length表示位长度，value
	struct BitString
	{
		int length;	
		int value;
	};

	BitString m_Y_DC_Huffman_Table[12];
	BitString m_Y_AC_Huffman_Table[256];

	BitString m_CbCr_DC_Huffman_Table[12];
	BitString m_CbCr_AC_Huffman_Table[256];

private:
	void _initHuffmanTables(void);
	void _initQualityTables(int quality);
	void _computeHuffmanTable(const char* nr_codes, const unsigned char* std_table, BitString* huffman_table);
	BitString _getBitCode(int value);

	void _convertColorSpace(const unsigned char* rgbBuffer, char* yData, char* cbData, char* crData);
	void _foword_FDC(const char* channel_data, short* fdc_data, const unsigned char* quant_table);
	void _doHuffmanEncoding(const short* DU, short& prevDC, const BitString* HTDC, const BitString* HTAC, 
		BitString* outputBitString, int& bitStringCounts);

private:
	void _write_jpeg_header(FILE* fp);
	void _write_byte_(unsigned char value, FILE* fp);
	void _write_word_(unsigned short value, FILE* fp);
	void _write_bitstring_(const BitString* bs, int counts, int& newByte, int& newBytePos, FILE* fp);
	void _write_(const void* p, int byteSize, FILE* fp);

public:
	//构造方法~
	JpegEncoder();
	~JpegEncoder();
};

#endif
