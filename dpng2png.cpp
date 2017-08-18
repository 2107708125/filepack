//by �y����Լ��
//github https://github.com/hz86/filepack

#include <io.h>
#include <tchar.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <Windows.h>
#include <Shlwapi.h>
#include <gdiplus.h>
using namespace Gdiplus;

#pragma comment(lib, "Gdiplus.lib") 
#pragma comment(lib, "Shlwapi.lib") 

typedef struct DPNGHEAD {
	unsigned char signature[4];
	unsigned int  unknown1;
	unsigned int  entry_count;
	unsigned int  width;
	unsigned int  height;
} DPNGHEAD;

typedef struct DPNGENTRY {
	unsigned int offset_x;
	unsigned int offset_y;
	unsigned int width;
	unsigned int height;
	unsigned int length;
	unsigned int unknown1;
	unsigned int unknown2;
} DPNGENTRY;

//�����ļ�
static unsigned char * get_file(wchar_t *file, unsigned int *len)
{
	unsigned char *ret = NULL;
	FILE *fp = _wfopen(file, L"rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		unsigned int fplen = ftell(fp);

		fseek(fp, 0, SEEK_SET);
		ret = (unsigned char *)malloc(fplen);
		fread(ret, 1, fplen, fp);

		*len = fplen;
		fclose(fp);
	}

	return ret;
}

//д���ļ�
static void put_file(wchar_t *file, unsigned char *data, unsigned int len)
{
	wchar_t dir[1024];
	wchar_t *pos = file;

	while (1)
	{
		pos = wcschr(pos, '\\');
		if (NULL == pos)
		{
			break;
		}

		wcsncpy(dir, file, pos - file);
		dir[pos - file] = 0;
		_wmkdir(dir);
		pos++;
	}

	FILE *fp = _wfopen(file, L"wb");
	fwrite(data, 1, len, fp);
	fclose(fp);
}

//GDI+ 
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;  // Failure

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return -1;  // Failure

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

//ת��
static int dpng_to_png(unsigned char *in, unsigned int len, unsigned char **out, unsigned int *outlen)
{
	int ret = -1;
	ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	unsigned int pos = 0;
	DPNGHEAD *dpng = (DPNGHEAD *)(in + pos);
	if (0 == memcmp(dpng->signature, "DPNG", 4))
	{
		Bitmap* bitmap = new Bitmap(dpng->width, dpng->height, PixelFormat32bppARGB);
		Graphics *graphics = new Graphics(bitmap);

		pos += sizeof(DPNGHEAD);
		for (unsigned int i = 0; i < dpng->entry_count; i++)
		{
			DPNGENTRY *entry = (DPNGENTRY *)(in + pos);
			pos += sizeof(DPNGENTRY);

			//wchar_t file[1024];
			//swprintf(file, L"%d.png", i);
			//put_file(file, in + pos, entry->length);

			IStream *entry_stream = SHCreateMemStream(in + pos, entry->length);
			Image* image = new Image(entry_stream);
			pos += entry->length;

			Rect rect;
			rect.X = entry->offset_x;
			rect.Y = entry->offset_y;
			rect.Width = entry->width;
			rect.Height = entry->height;
			graphics->DrawImage(image, rect);

			delete image;
			entry_stream->Release();
		}

		delete graphics;

		CLSID format;
		GetEncoderClsid(L"image/png", &format);
		IStream *out_stream = SHCreateMemStream(NULL, 0);
		bitmap->Save(out_stream, &format);

		ULARGE_INTEGER stream_len;
		LARGE_INTEGER stream_pos = { 0 };
		out_stream->Seek(stream_pos, STREAM_SEEK_END, &stream_len);
		out_stream->Seek(stream_pos, STREAM_SEEK_SET, NULL);

		ULONG readlen;
		*out = (unsigned char *)malloc(stream_len.LowPart);
		out_stream->Read(*out, stream_len.LowPart, &readlen);
		*outlen = readlen;

		out_stream->Release();
		delete bitmap;
		ret = 0;
	}

	GdiplusShutdown(gdiplusToken);
	return ret;
}

//ͨ����Ƚ�
int match_with_asterisk(wchar_t* str1, wchar_t* pattern)
{
	if (str1 == NULL) {
		return -1;
	}

	if (pattern == NULL) {
		return -1;
	}

	int len1 = wcslen(str1);
	int len2 = wcslen(pattern);
	int p1 = 0, p2 = 0;

	//���ڷֶα��,'*'�ָ����ַ���
	int mark = 0;
	
	while (p1 < len1 && p2 < len2)
	{
		if (pattern[p2] == '?')
		{
			p1++; p2++;
			continue;
		}
		if (pattern[p2] == '*')
		{
			/*�����ǰ��*�ţ���markǰ��һ�����Ѿ����ƥ�䣬
			*�ӵ�ǰ�㿪ʼ������һ�����ƥ��
			*/
			p2++;
			mark = p2;
			continue;
		}
		if (str1[p1] != pattern[p2])
		{
			if (p1 == 0 && p2 == 0)
			{
				/*
				* ��������ַ������⴦������ͬ��ƥ��ʧ��
				*/
				return -1;
			}
			/*
			* pattern: ...*bdef*...
			*       ^
			*       mark
			*        ^
			*        p2
			*       ^
			*       new p2
			* str1:.....bdcf...
			*       ^
			*       p1
			*      ^
			*     new p1
			* ����ʾ��ͼ��ʾ���ڱȵ�e��c�������
			* p2���ص�mark����
			* p1��Ҫ���ص���һ��λ�á�
			* ��Ϊ*ǰ�Ѿ����ƥ�䣬����mark���֮ǰ�Ĳ���Ҫ�ٱȽ�
			*/
			p1 -= p2 - mark - 1;
			p2 = mark;
			continue;
		}
		/*
		* �˴�������ȵ����
		*/
		p1++;
		p2++;
	}
	if (p2 == len2)
	{
		if (p1 == len1)
		{
			/*
			* �����ַ����������ˣ�˵��ģʽƥ��ɹ�
			*/
			return 0;
		}
		if (pattern[p2 - 1] == '*')
		{
			/*
			* str1��û�н�������pattern�����һ���ַ���*������ƥ��ɹ�
			*
			*/
			return 0;
		}
	}
	while (p2 < len2)
	{
		/*
		* pattern������ַ�ֻҪ��һ������*,ƥ��ʧ��
		*
		*/
		if (pattern[p2] != '*') {
			return -1;
		}
		p2++;
	}

	return -1;
}

//ö���ļ�
static void enum_file(wchar_t *in_path, wchar_t *findname, wchar_t **out_filename[], int *out_size)
{
	wchar_t filename[1024];
	wcscat(wcscpy(filename, in_path), L"\\*");

	struct _wfinddata64_t data;
	intptr_t handle = _wfindfirst64(filename, &data);
	if (-1 != handle)
	{
		do
		{
			if (0 == wcscmp(L".", data.name) || 0 == wcscmp(L"..", data.name)) {
				continue;
			}

			if (_A_SUBDIR == (data.attrib & _A_SUBDIR)) {
				wcscat(wcscat(wcscpy(filename, in_path), L"\\"), data.name);
				enum_file(filename, findname, out_filename, out_size);
			}
			else
			{
				if (0 == match_with_asterisk(data.name, findname))
				{
					if (NULL == *out_filename) {
						*out_filename = (wchar_t **)malloc(sizeof(wchar_t *) * (*out_size + 1));
					}
					else {
						*out_filename = (wchar_t **)realloc(*out_filename, sizeof(wchar_t *) * (*out_size + 1));
					}

					(*out_filename)[*out_size] = (wchar_t *)malloc(wcslen(in_path) * 2 + 2 + wcslen(data.name) * 2 + 2);
					wcscat(wcscat(wcscpy((*out_filename)[*out_size], in_path), L"\\"), data.name);
					(*out_size)++;
				}
			}
		}
		while (0 == _tfindnext64(handle, &data));
		_findclose(handle);
	}
}

int wmain(int argc, wchar_t* argv[])
{
	_wsetlocale(LC_ALL, L"chs");

	if (1 == argc)
	{
		wprintf(L"����Ů���A�R ����P����Ů [dpng to png] tool\r\n\r\n");

		wprintf(L"help \r\n");
		wprintf(L"dpng2png -f in.png out.png #ת�������ļ�\r\n");
		wprintf(L"dpng2png -a ./path         #����ת��������ԭ�ļ�\r\n");
	}
	else if (3 == argc)
	{
		if (0 == wcscmp(L"-a", argv[1]))
		{
			int len = 0;
			wchar_t **arr = NULL;
			enum_file(argv[2], L"*.png", &arr, &len);

			if (arr)
			{
				for (int i = 0; i < len; i++)
				{
					wprintf(L"%s\r\n", arr[i] + wcslen(argv[2]));

					unsigned int datalen;
					unsigned char *data = get_file(arr[i], &datalen);

					unsigned char *out;
					unsigned int outlen;
					if (-1 != dpng_to_png(data, len, &out, &outlen))
					{
						put_file(arr[i], out, outlen);
						free(out);
					}

					free(data);
				}

				for (int i = 0; i < len; i++) {
					free(arr[i]);
				}

				free(arr);
			}
		}
	}
	else if (4 == argc)
	{
		if (0 == wcscmp(L"-f", argv[1]))
		{
			unsigned int len;
			unsigned char *data = get_file(argv[2], &len);

			unsigned char *out;
			unsigned int outlen;
			if (-1 != dpng_to_png(data, len, &out, &outlen))
			{
				put_file(argv[3], out, outlen);
				free(out);
			}

			free(data);
		}
	}

	return 0;
}

