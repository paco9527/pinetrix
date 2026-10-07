#include "graph_priv.h"
//#include "gfxfont.h"//不能放这里,头文件中有相关引用
#include "TomThumb.h"//gfxfont.h中有结构体定义，放在它后面
#include "log.h"

typedef struct _GRAPH_HDL
{
    int width;
    int height;
    uint32_t color;
    uint8_t brightness;
    GFXfont *gfxFont;
    uint32_t *buffer;
}GRAPH_HDL;

static void print_Gram(uint32_t *buffer)
{
	uint32_t size = 32 * 8;
	uint32_t count = 0;
	for( ; count < size ; count++)
	{
		if(count % 32)//到一行末尾
		{
			if(buffer[count])//非0
				LOG_DEBUG("%0x,\t", buffer[count]);
			else
				LOG_DEBUG("0,\t");
		}
		else
			LOG_DEBUG("");
	}
}

//计算对应的下标
uint16_t getpos(GRAPH_HDL* hdl, uint16_t x,uint16_t y)
{
	uint16_t pos;
	if(x%2)//奇数列
	{
		pos = (x+1) * hdl->height - 1 - y;
	}
	else//偶数列
	{
		pos = x * hdl->height + y;
	}
	return pos;
}

void setFont(void* hdl, const GFXfont *f)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
    graph_hdl->gfxFont = (GFXfont *)f;
}
		 
//清屏函数
void Clear(void* hdl)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	uint32_t lastcolor = graph_hdl->color;
	uint32_t count = 0;
	memset(graph_hdl->buffer, 0, graph_hdl->width * graph_hdl->height * sizeof(PIXEL_TYPE));
	graph_hdl->color = lastcolor;
}

void setPixelColorHex(void* hdl, uint32_t c)
{
	//BGR
	uint8_t r = (c>>16)&0xff;
	uint8_t g = (c>>8)&0xff;
	uint8_t b = c&0xff;
	setPixelColor(hdl,r,g,b);
}

void setPixelColor(void* hdl, uint8_t rset, uint8_t gset, uint8_t bset)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
#ifndef USE_HW_BRIGHTNESS
	rset=(uint8_t)(rset*(graph_hdl->brightness/100.0));
	gset=(uint8_t)(gset*(graph_hdl->brightness/100.0));
	bset=(uint8_t)(bset*(graph_hdl->brightness/100.0));
	graph_hdl->color = (rset << 16) | (gset << 8) | bset;//WRGB,RGB888
#else
	graph_hdl->color = (rset << 16) | (gset << 8) | bset;//WRGB,RGB888
#endif
}

uint8_t graph_getbrightness(void* hdl)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	return graph_hdl->brightness;
}

void graph_setbrightness(void* hdl, uint8_t b)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	graph_hdl->brightness = b;
}

//画点 
void writePixel(void* hdl, uint16_t x,uint16_t y)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	uint16_t pos;
	if(x>=graph_hdl->width||y>=graph_hdl->height)
	{
//		LOG_ERROR("out of range x:%u, y:%u\n", x, y);
		return;//超出范围了,截断了不画
	}
	pos = getpos(graph_hdl,x,y);
	graph_hdl->buffer[pos] = graph_hdl->color;
}

//快速水平线/垂直线
void drawFastVLine(void* hdl, uint16_t x, uint16_t y, int16_t h)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	if(h > 0)//向下延伸
	{
		for(y;(y<graph_hdl->height)&&h;y++,h--)
			writePixel(hdl,x,y);
	}
	else//向上延伸
	{
		for(y;(y>0)&&h;y--,h++)
			writePixel(hdl,x,y);
	}
}

void drawFastHLine(void* hdl, uint16_t x, uint16_t y, int16_t w)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	if(w > 0)//向右延伸
	{
		for(x;(x<graph_hdl->width)&&w;x++,w--)
			writePixel(hdl,x,y);
	}
	else//向左延伸
	{
		for(x;(x>0)&&w;x--,w++)
			writePixel(hdl,x,y);
	}
}

//x1,y1,x2,y2 填充区域的对角坐标	  
void writeFillRect(void* hdl,uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2)
{  
	uint16_t x;
	for(x=x1;x<x2;x++)
		drawFastVLine(hdl, x, y1, y2-y1);
}

int16_t drawChar(void* hdl, int16_t x, int16_t y, unsigned char c) 
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	GFXfont* p_font = graph_hdl->gfxFont;
	c -= 0x20;
	uint32_t lastcolor = graph_hdl->color;
	int16_t xcursor;
	int16_t ycursor = 0;
	uint16_t bitmapindex = p_font->glyph[c].bitmapOffset;//读出在一维数组中的位置
	uint8_t bitmaplen = p_font->glyph[c].height;
	int16_t yoffset = (p_font->glyph[c].yOffset) + 5;//+5来自字体最大宽度
	int16_t width;
	uint8_t tmp;
	int16_t returnwidth = 0;
	for(;ycursor<bitmaplen;ycursor++)
	{
		tmp = p_font->bitmap[bitmapindex];
		width = p_font->glyph[c].width;
		for(xcursor = 0; xcursor< width; xcursor++)
		{
			if(x+xcursor > graph_hdl->width)
				break;
			if(tmp & 0x80)
			{
				writePixel(hdl,x+xcursor, y+ycursor+yoffset);
				if(returnwidth < xcursor)
					returnwidth = xcursor;
			}
			else
			{
				graph_hdl->color = 0;
				writePixel(hdl,x+xcursor, y+ycursor+yoffset);
				graph_hdl->color = lastcolor;
			}
			tmp <<= 1;
		}
		bitmapindex++;
	}
	
	return returnwidth+CHARPADDING;
}

void drawString(void* hdl, int16_t x, int16_t y, const char *s)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	int16_t xoffset = 0;
	int16_t width = 0;
	while(*s!='\0')
	{
		if(*s == '\n' || *s == '\r')
			return;
		width = drawChar(hdl,x+xoffset, y, *s);
		xoffset+=width;
		s++;
	}
	print_Gram(graph_hdl->buffer);
}

void drawBitmap(void* hdl, int16_t x, int16_t y, org_pixel_data* bmp, int16_t xsize, int16_t ysize)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	int16_t picx = 0;
	int16_t picy = 0;
	int16_t cursorx = x;
	int16_t cursory = y;
	uint color = graph_hdl->color;
	uint lastcolor = graph_hdl->color;
	for(;picy<ysize;picy++)
	{
		cursory = y+picy;
		cursorx = x;
		for(picx=0;picx<xsize;picx++)
		{
			color = bmp[picx+picy*ysize].data->rgb888.red << 16 | \
					bmp[picx+picy*ysize].data->rgb888.green << 8 | \
					bmp[picx+picy*ysize].data->rgb888.blue;
			writePixel(hdl, cursorx, cursory);
			cursorx++;
		}
	}
	color = lastcolor;
}

//长 宽 uint32显存数组
void* graph_init(uint16_t w, uint16_t h)
{
#if 0
	GRAPH_HDL* graph_hdl = malloc(sizeof(GRAPH_HDL));
	if(!graph_hdl)
	{
		LOG_ERROR("malloc handle error\n");
		return NULL;
	}
#endif
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)get_graph_instance();
//	memset(graph_hdl, 0, sizeof(GRAPH_HDL));
	LOG_INFO("test print");
	//graph_hdl->buffer = malloc(sizeof(uint32_t)*w*h);
	//if(graph_hdl->buffer == NULL)
	//{
	//	LOG_ERROR("malloc buffer error\n");
	//	return NULL;
	//}
	graph_hdl->width = w;
	graph_hdl->height = h;
	graph_hdl->brightness = 80;
	setPixelColor((GRAPH)graph_hdl,0,60,30);//调试,后续改成外部配置
	setFont((void*)graph_hdl,&TomThumb);
	LOG_INFO("GRAPH construct completed\n");
	return graph_hdl;
}

void graph_deinit(void* hdl)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	//后续区分缓存来源,是驱动申请 自行申请还是多个缓冲
//	free(graph_hdl->buffer);
	graph_hdl->buffer = NULL;
//	free(graph_hdl);
	LOG_INFO("GRAPH destruct completed\n");
}

uint32_t* graph_getbuffer(void* hdl)
{
	GRAPH_HDL* graph_hdl = (GRAPH_HDL*)hdl;
	return graph_hdl->buffer;
}

GRAPH get_graph_instance(void)
{
	static GRAPH_HDL graph_inst = {0};
	return (GRAPH)(&graph_inst);
}

void graph_set_buffer(void* hdl, uint32_t* buf)
{
	GRAPH_HDL* graph_hdl = get_graph_instance();
	graph_hdl->buffer = buf;
}


