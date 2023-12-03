#ifndef __JPEG_ENCODER_HEADER__
#define __JPEG_ENCODER_HEADER__

// ��BMP��ʽ��ͼ��ת��ΪJPEG��ʽ
class JpegEncoder
{


	//�洢�����õķ���
public:
	/** �������� */
	void clean(void);

	/** ��BMP�ļ��ж�ȡ�ļ�����֧��24bit��ͼ��ĳߴ糤�ȱ�����8�ı������ļ� */
	bool readFromBMP(const char* fileName);

	/** ѹ����jpg�ļ��У�quality_scale��ʾ������ȡֵ��Χ(0,100), ����Խ��ѹ������Խ��*/
	bool encodeToJPG(const char* fileName, int quality_scale);

private:
	//����˽�б���
	//ͼ����������
	int				m_width;
	int				m_height;
	//�洢��BMP�ļ���ȡ��RGB��ʽ������
	unsigned char*	m_rgbBuffer;
	//�洢��׼���ȷ�����������
	unsigned char	m_YTable[64];
	//�洢��׼ɫ���������
	unsigned char	m_CbCrTable[64];
	// �洢��ʾ�����������codeword��length��ʾλ���ȣ�value
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
	//���췽��~
	JpegEncoder();
	~JpegEncoder();
};

#endif
