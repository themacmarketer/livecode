#include "graphics.h"
#include "graphics-internal.h"

#include "SkDevice.h"

////////////////////////////////////////////////////////////////////////////////

static inline uint32_t packed_bilinear_bounded(uint32_t x, uint8_t a, uint32_t y, uint8_t b)
{
	uint32_t u, v;

	u = (x & 0xff00ff) * a + (y & 0xff00ff) * b + 0x800080;
	u = ((u + ((u >> 8) & 0xff00ff)) >> 8) & 0xff00ff;

	v = ((x >> 8) & 0xff00ff) * a + ((y >> 8) & 0xff00ff) * b + 0x800080;
	v = (v + ((v >> 8) & 0xff00ff)) & 0xff00ff00;

	return u | v;
}

////////////////////////////////////////////////////////////////////////////////

static bool w32_draw_text_using_mask_to_context_at_device_location(MCGContextRef p_context, const unichar_t *p_text, uindex_t p_length, MCGPoint p_location, MCGIntRectangle p_bounds, HDC p_gdicontext)
{
	bool t_success;
	t_success = true;

	void *t_rgb_data;
	t_rgb_data = NULL;
	HBITMAP t_rgb_bitmap;
	t_rgb_bitmap = NULL;
	if (t_success)
	{
		BITMAPINFO t_bitmapinfo;
		MCMemoryClear(&t_bitmapinfo, sizeof(BITMAPINFO));
		t_bitmapinfo . bmiHeader . biSize = sizeof(BITMAPINFOHEADER);
		t_bitmapinfo . bmiHeader . biCompression = BI_RGB;
		t_bitmapinfo . bmiHeader . biWidth = p_bounds . width;
		t_bitmapinfo . bmiHeader . biHeight = -p_bounds . height;
		t_bitmapinfo . bmiHeader . biPlanes = 1;
		t_bitmapinfo . bmiHeader . biBitCount = 32;
		t_rgb_bitmap = CreateDIBSection(p_gdicontext, &t_bitmapinfo, DIB_RGB_COLORS, &t_rgb_data, NULL, 0);
		t_success = t_rgb_bitmap != NULL && t_rgb_data != NULL;
	}

	if (t_success)
		t_success = SelectObject(p_gdicontext, t_rgb_bitmap) != NULL;

	uint32_t *t_rgb_pxls, *t_context_pxls;
	uint32_t t_context_width, t_x_offset, t_y_offset;
	if (t_success)
	{
		t_rgb_pxls = (uint32_t *) t_rgb_data;

		t_context_pxls = (uint32_t *) p_context -> layer -> canvas -> getTopDevice() -> accessBitmap(false) . getPixels();
		t_context_width = p_context -> layer -> canvas -> getTopDevice() -> accessBitmap(false) . rowBytes() / 4;	

		t_x_offset = p_location . x + p_bounds . x;
		t_y_offset = p_location . y + p_bounds . y;

		for(uint32_t y = 0; y < p_bounds . height; y++)
		{
			for(uint32_t x = 0; x < p_bounds . width; x++)
			{
				uint32_t t_context_pxl;
				t_context_pxl = *(t_context_pxls + (y + t_y_offset) * t_context_width + x + t_x_offset);
				if (((t_context_pxl >> 24) & 0xFF) == 0xFF)
					*(t_rgb_pxls + y * p_bounds . width + x) = t_context_pxl;
				else
					*(t_rgb_pxls + y * p_bounds . width + x) = 0x00FFFFFF;
			}
		}

		SetBkMode(p_gdicontext, TRANSPARENT);
		SetTextColor(p_gdicontext, RGB((p_context -> state -> fill_color >> 16) & 0xFF, (p_context -> state -> fill_color >> 8) & 0xFF, (p_context -> state -> fill_color >> 0) & 0xFF));
		t_success = TextOutW(p_gdicontext, 0, 0, (LPCWSTR)p_text, p_length >> 1);
	}

	if (t_success)
		t_success = GdiFlush();

	if (t_success)
	{
		for(uint32_t y = 0; y < p_bounds . height; y++)
		{
			for(uint32_t x = 0; x < p_bounds . width; x++)
			{
				uint32_t *t_context_pxl;
				t_context_pxl = t_context_pxls + (y + t_y_offset) * t_context_width + x + t_x_offset;
				if (((*t_context_pxl >> 24) & 0xFF) == 0xFF)
					*t_context_pxl = *(t_rgb_pxls + y * p_bounds . width + x) | 0xFF000000;
				else
				{
					uint32_t t_lcd32_val;
					t_lcd32_val = *(t_rgb_pxls + y * p_bounds . width + x);
					
					uint8_t t_a8_val;
					t_a8_val = 255 - ((t_lcd32_val & 0xFF) + ((t_lcd32_val & 0xFF00) >> 8) + ((t_lcd32_val & 0xFF0000) >> 16)) / 3;
					
					*t_context_pxl = packed_bilinear_bounded(*t_context_pxl, 255 - t_a8_val, p_context -> state -> fill_color, t_a8_val);;
				}
			}
		}
	}

	DeleteObject(t_rgb_bitmap);
	return t_success;
}

static bool w32_draw_text_to_context_at_device_location(MCGContextRef p_context, const unichar_t *p_text, uindex_t p_length, MCGPoint p_location, MCGIntRectangle p_bounds, HDC p_gdicontext)
{
	bool t_success;
	t_success = true;

	void *t_dib_data;
	t_dib_data = NULL;
	HBITMAP t_gdibitmap;
	t_gdibitmap = NULL;
	if (t_success)
	{
		BITMAPINFO t_bitmapinfo;
		MCMemoryClear(&t_bitmapinfo, sizeof(BITMAPINFO));
		t_bitmapinfo . bmiHeader . biSize = sizeof(BITMAPINFOHEADER);
		t_bitmapinfo . bmiHeader . biCompression = BI_RGB;
		t_bitmapinfo . bmiHeader . biWidth = p_bounds . width;
		t_bitmapinfo . bmiHeader . biHeight = -p_bounds . height;
		t_bitmapinfo . bmiHeader . biPlanes = 1;
		t_bitmapinfo . bmiHeader . biBitCount = 32;
		t_gdibitmap = CreateDIBSection(p_gdicontext, &t_bitmapinfo, DIB_RGB_COLORS, &t_dib_data, NULL, 0);
		t_success = t_gdibitmap != NULL && t_dib_data != NULL;
	}

	if (t_success)
		t_success = SelectObject(p_gdicontext, t_gdibitmap) != NULL;	

	if (t_success)
	{
		SetTextColor(p_gdicontext, 0x00000000);
		SetBkColor(p_gdicontext, 0x00FFFFFF);
		SetBkMode(p_gdicontext, OPAQUE);		
		t_success = TextOutW(p_gdicontext, 0, 0, (LPCWSTR)p_text, p_length >> 1);
	}
	
	if (t_success)
		t_success = GdiFlush();
	
	void *t_data;
	t_data = NULL;
	if (t_success)
		t_success = MCMemoryNew(p_bounds . width * p_bounds . height, t_data);
	
	if (t_success)
	{
		uint32_t *t_src_data;
		t_src_data = (uint32_t *) t_dib_data;
		
		uint8_t *t_dst_data;
		t_dst_data = (uint8_t *) t_data;
		
		for(uint32_t y = 0; y < p_bounds . height; y++)
		{
			for(uint32_t x = 0; x < p_bounds . width; x++)
			{
				uint32_t t_lcd32_val;
				t_lcd32_val = *(t_src_data + y * p_bounds . width + x);		
				
				uint8_t t_a8_val;
				t_a8_val = ((t_lcd32_val & 0xFF) + ((t_lcd32_val & 0xFF00) >> 8) + ((t_lcd32_val & 0xFF0000) >> 16)) / 3;
				
				*(t_dst_data + y * p_bounds . width + x) = 255 - t_a8_val;
			}
		}
		
		SkPaint t_paint;
		t_paint . setStyle(SkPaint::kFill_Style);	
		t_paint . setAntiAlias(p_context -> state -> should_antialias);
		t_paint . setColor(MCGColorToSkColor(p_context -> state -> fill_color));
		
		SkXfermode *t_blend_mode;
		t_blend_mode = MCGBlendModeToSkXfermode(p_context -> state -> blend_mode);
		t_paint . setXfermode(t_blend_mode);
		if (t_blend_mode != NULL)
			t_blend_mode -> unref();		
		
		SkBitmap t_bitmap;
		t_bitmap . setConfig(SkBitmap::kA8_Config, p_bounds . width, p_bounds . height);
		t_bitmap . setIsOpaque(false);
		t_bitmap . setPixels(t_data);
		p_context -> layer -> canvas -> drawSprite(t_bitmap, p_bounds . x + p_location . x, 
											  p_bounds . y + p_location . y, &t_paint);
	}
	
	DeleteObject(t_gdibitmap);
	MCMemoryDelete(t_data);
	return t_success;
}

static bool w32_draw_opaque_text_to_context_at_device_location(MCGContextRef p_context, const unichar_t *p_text, uindex_t p_length, MCGPoint p_location, MCGIntRectangle p_bounds, HDC p_gdicontext)
{
	bool t_success;
	t_success = true;

	void *t_dib_data;
	t_dib_data = NULL;
	HBITMAP t_gdibitmap;
	t_gdibitmap = NULL;
	if (t_success)
	{
		BITMAPINFO t_bitmapinfo;
		MCMemoryClear(&t_bitmapinfo, sizeof(BITMAPINFO));
		t_bitmapinfo . bmiHeader . biSize = sizeof(BITMAPINFOHEADER);
		t_bitmapinfo . bmiHeader . biCompression = BI_RGB;
		t_bitmapinfo . bmiHeader . biWidth = p_bounds . width;
		t_bitmapinfo . bmiHeader . biHeight = -p_bounds . height;
		t_bitmapinfo . bmiHeader . biPlanes = 1;
		t_bitmapinfo . bmiHeader . biBitCount = 32;
		t_gdibitmap = CreateDIBSection(p_gdicontext, &t_bitmapinfo, DIB_RGB_COLORS, &t_dib_data, NULL, 0);
		t_success = t_gdibitmap != NULL && t_dib_data != NULL;
	}

	if (t_success)
		t_success = SelectObject(p_gdicontext, t_gdibitmap) != NULL;	

	uint32_t *t_context_pxls, *t_dib_pxls;
	uint32_t t_context_width, t_x_offset, t_y_offset;
	if (t_success)
	{
		t_context_pxls = (uint32_t *) p_context -> layer -> canvas -> getTopDevice() -> accessBitmap(false) . getPixels();
		t_dib_pxls = (uint32_t *) t_dib_data;

		t_context_width = p_context -> layer-> canvas -> getTopDevice() -> accessBitmap(false) . rowBytes() / 4;		
		t_x_offset = p_location . x + p_bounds . x;
		t_y_offset = p_location . y + p_bounds . y;

		for(uint32_t y = 0; y < p_bounds . height; y++)
			for(uint32_t x = 0; x < p_bounds . width; x++)
				*(t_dib_pxls + y * p_bounds . width + x) = *(t_context_pxls + (y + t_y_offset) * t_context_width + x + t_x_offset);

		SetBkMode(p_gdicontext, TRANSPARENT);
		SetTextColor(p_gdicontext, RGB((p_context -> state -> fill_color >> 16) & 0xFF, (p_context -> state -> fill_color >> 8) & 0xFF, (p_context -> state -> fill_color >> 0) & 0xFF));
		t_success = TextOutW(p_gdicontext, 0, 0, (LPCWSTR)p_text, p_length >> 1);
	}	

	if (t_success)
		t_success = GdiFlush();

	if (t_success)
	{
		for(uint32_t y = 0; y < p_bounds . height; y++)
			for(uint32_t x = 0; x < p_bounds . width; x++)
				*(t_context_pxls + (y + t_y_offset) * t_context_width + x + t_x_offset) = *(t_dib_pxls + y * p_bounds . width + x) | 0xFF000000;
	}

	return t_success;
}

////////////////////////////////////////////////////////////////////////////////

void MCGContextDrawPlatformText(MCGContextRef self, const unichar_t *p_text, uindex_t p_length, MCGPoint p_location, const MCGFont &p_font)
{
	if (!MCGContextIsValid(self))
		return;	
	
	bool t_success;
	t_success = true;
	
	HDC t_gdicontext;
	t_gdicontext = NULL;
	if (t_success)
	{
		t_gdicontext = CreateCompatibleDC(NULL);
		t_success = t_gdicontext != NULL;
	}
	
	if (t_success)
		t_success = SetGraphicsMode(t_gdicontext, GM_ADVANCED) != 0;
	
	if (t_success)
		t_success = SelectObject(t_gdicontext, p_font . fid) != NULL;
	
	SIZE t_size;
	if (t_success)
		t_success = GetTextExtentPoint32W(t_gdicontext, (LPCWSTR)p_text, p_length >> 1, &t_size);
	
	TEXTMETRICA t_metrics;
	if (t_success)
		t_success = GetTextMetricsA(t_gdicontext, &t_metrics);
	
	MCGIntRectangle t_text_bounds, t_clipped_bounds;
	MCGAffineTransform t_transform;
	MCGPoint t_device_location;
	if (t_success)
	{
		MCGRectangle t_float_text_bounds;
		t_float_text_bounds . origin . x = 0;
		t_float_text_bounds . origin . y = -t_metrics . tmAscent;
		t_float_text_bounds . size . width = t_size . cx;
		t_float_text_bounds . size . height = t_metrics . tmAscent + t_metrics . tmDescent;
		
		t_transform = MCGContextGetDeviceTransform(self);
		t_device_location = MCGPointApplyAffineTransform(p_location, t_transform);		
		t_transform . tx = modff(t_device_location . x, &t_device_location . x);
		t_transform . ty = modff(t_device_location . y, &t_device_location . y);
		
		MCGRectangle t_device_clip;
		t_device_clip = MCGContextGetDeviceClipBounds(self);
		t_device_clip . origin . x -= t_device_location . x;
		t_device_clip . origin . y -= t_device_location . y;
		
		MCGRectangle t_float_clipped_bounds;
		t_float_text_bounds = MCGRectangleApplyAffineTransform(t_float_text_bounds, t_transform);		
		t_float_clipped_bounds = MCGRectangleIntersection(t_float_text_bounds, t_device_clip);
		
		t_text_bounds = MCGRecangleIntegerBounds(t_float_text_bounds);
		t_clipped_bounds = MCGRecangleIntegerBounds(t_float_clipped_bounds);
		
		if (t_clipped_bounds . width == 0 || t_clipped_bounds . height == 0)
		{
			DeleteDC(t_gdicontext);
			return;
		}
	}
	
	if (t_success)
	{
		XFORM t_xform;
		t_xform . eM11 = t_transform . a;
		t_xform . eM12 = t_transform . b;
		t_xform . eM21 = t_transform . c;
		t_xform . eM22 = t_transform . d;
		t_xform . eDx = t_transform . tx - (t_clipped_bounds . x - t_text_bounds . x);
		t_xform . eDy = t_transform . ty - (t_clipped_bounds . y - t_text_bounds . y);
		t_success = SetWorldTransform(t_gdicontext, &t_xform);
	}

	if (t_success)
		t_success = w32_draw_text_using_mask_to_context_at_device_location(self, p_text, p_length, t_device_location, t_clipped_bounds, t_gdicontext);
		//t_success = w32_draw_opaque_text_to_context_at_device_location(self, p_text, p_length, t_device_location, t_clipped_bounds, t_gdicontext);
		//t_success = w32_draw_text_to_context_at_device_location(self, p_text, p_length, t_device_location, t_clipped_bounds, t_gdicontext);

	DeleteDC(t_gdicontext);
	self -> is_valid = t_success;
}

MCGFloat MCGContextMeasurePlatformText(MCGContextRef self, const unichar_t *p_text, uindex_t p_length, const MCGFont &p_font)
{
	if (!MCGContextIsValid(self))
		return 0.0;	
	
	bool t_success;
	t_success = true;
	
	HDC t_gdicontext;
	t_gdicontext = NULL;
	if (t_success)
	{
		t_gdicontext = CreateCompatibleDC(NULL);
		t_success = t_gdicontext != NULL;
	}
	
	if (t_success)
		t_success = SetGraphicsMode(t_gdicontext, GM_ADVANCED) != 0;
	
	if (t_success)
		t_success = SelectObject(t_gdicontext, p_font . fid) != NULL;
	
	SIZE t_size;
	if (t_success)
		t_success = GetTextExtentPoint32W(t_gdicontext, (LPCWSTR)p_text, p_length >> 1, &t_size);
	
	DeleteDC(t_gdicontext);
	self -> is_valid = t_success;
	
	if (t_success)
		return t_size . cx;
	else
		return 0.0;
}

////////////////////////////////////////////////////////////////////////////////

#if 0
void MCGContextDrawPlatformText(MCGContextRef self, const unichar_t *p_text, uindex_t p_length, MCGPoint p_location, const MCGFont &p_font)
{
	if (!MCGContextIsValid(self))
		return;	
	
	bool t_success;
	t_success = true;
	
	HDC t_gdicontext;
	t_gdicontext = NULL;
	if (t_success)
	{
		t_gdicontext = CreateCompatibleDC(NULL);
		t_success = t_gdicontext != NULL;
	}
	
	if (t_success)
		t_success = SetGraphicsMode(t_gdicontext, GM_ADVANCED) != 0;
	
	if (t_success)
		t_success = SelectObject(t_gdicontext, p_font . fid) != NULL;
	
	SIZE t_size;
	if (t_success)
		t_success = GetTextExtentPoint32W(t_gdicontext, (LPCWSTR)p_text, p_length >> 1, &t_size);
	
	TEXTMETRICA t_metrics;
	if (t_success)
		t_success = GetTextMetricsA(t_gdicontext, &t_metrics);
	
	MCGIntRectangle t_text_bounds, t_clipped_bounds;
	MCGAffineTransform t_transform;
	MCGPoint t_device_location;
	if (t_success)
	{
		MCGRectangle t_float_text_bounds;
		t_float_text_bounds . origin . x = 0;
		t_float_text_bounds . origin . y = -t_metrics . tmAscent;
		t_float_text_bounds . size . width = t_size . cx;
		t_float_text_bounds . size . height = t_metrics . tmAscent + t_metrics . tmDescent;
		
		t_transform = MCGContextGetDeviceTransform(self);
		t_device_location = MCGPointApplyAffineTransform(p_location, t_transform);		
		t_transform . tx = modff(t_device_location . x, &t_device_location . x);
		t_transform . ty = modff(t_device_location . y, &t_device_location . y);
		
		MCGRectangle t_device_clip;
		t_device_clip = MCGContextGetDeviceClipBounds(self);
		t_device_clip . origin . x -= t_device_location . x;
		t_device_clip . origin . y -= t_device_location . y;
		
		MCGRectangle t_float_clipped_bounds;
		t_float_text_bounds = MCGRectangleApplyAffineTransform(t_float_text_bounds, t_transform);		
		t_float_clipped_bounds = MCGRectangleIntersection(t_float_text_bounds, t_device_clip);
		
		t_text_bounds = MCGRecangleIntegerBounds(t_float_text_bounds);
		t_clipped_bounds = MCGRecangleIntegerBounds(t_float_clipped_bounds);
		
		if (t_clipped_bounds . width == 0 || t_clipped_bounds . height == 0)
		{
			DeleteDC(t_gdicontext);
			return;
		}
	}
	
	if (t_success)
	{
		XFORM t_xform;
		t_xform . eM11 = t_transform . a;
		t_xform . eM12 = t_transform . b;
		t_xform . eM21 = t_transform . c;
		t_xform . eM22 = t_transform . d;
		t_xform . eDx = t_transform . tx - (t_clipped_bounds . x - t_text_bounds . x);
		t_xform . eDy = t_transform . ty - (t_clipped_bounds . y - t_text_bounds . y);
		t_success = SetWorldTransform(t_gdicontext, &t_xform);
	}
	
	void *t_mask_data;
	t_mask_data = NULL;
	HBITMAP t_mask_bitmap;
	t_mask_bitmap = NULL;
	if (t_success)
	{
		BITMAPINFO t_bitmapinfo;
		MCMemoryClear(&t_bitmapinfo, sizeof(BITMAPINFO));
		t_bitmapinfo . bmiHeader . biSize = sizeof(BITMAPINFOHEADER);
		t_bitmapinfo . bmiHeader . biCompression = BI_RGB;
		t_bitmapinfo . bmiHeader . biWidth = t_clipped_bounds . width;
		t_bitmapinfo . bmiHeader . biHeight = -t_clipped_bounds . height;
		t_bitmapinfo . bmiHeader . biPlanes = 1;
		t_bitmapinfo . bmiHeader . biBitCount = 8;
		//t_bitmapinfo . bmiHeader . biSizeImage = t_clipped_bounds . height * ((t_clipped_bounds . width + 31) & ~31) / 8;

		t_bitmapinfo . bmiColors[0] . rgbRed = t_bitmapinfo . bmiColors[0] . rgbGreen = t_bitmapinfo . bmiColors[0] . rgbBlue = 0;
		t_bitmapinfo . bmiColors[1] . rgbRed = t_bitmapinfo . bmiColors[1] . rgbGreen = t_bitmapinfo . bmiColors[1] . rgbBlue = 0xFF;
		t_bitmapinfo . bmiColors[0] . rgbReserved = 0;
		t_bitmapinfo . bmiColors[1] . rgbReserved = 0;

		t_mask_bitmap = CreateDIBSection(t_gdicontext, &t_bitmapinfo, DIB_RGB_COLORS, &t_mask_data, NULL, 0);
		t_success = t_mask_bitmap != NULL && t_mask_data != NULL;
	}
	
	if (t_success)
		t_success = SelectObject(t_gdicontext, t_mask_bitmap) != NULL;

	if (t_success)
	{
		SetTextColor(t_gdicontext, 0x00000000);
		SetBkColor(t_gdicontext, 0x00FFFFFF);
		SetBkMode(t_gdicontext, OPAQUE);		
		t_success = TextOutW(t_gdicontext, 0, 0, (LPCWSTR)p_text, p_length >> 1);
	}
	
	if (t_success)
		t_success = GdiFlush();

	void *t_rgb_data;
	t_rgb_data = NULL;
	HBITMAP t_rgb_bitmap;
	t_rgb_bitmap = NULL;
	if (t_success)
	{
		BITMAPINFO t_bitmapinfo;
		MCMemoryClear(&t_bitmapinfo, sizeof(BITMAPINFO));
		t_bitmapinfo . bmiHeader . biSize = sizeof(BITMAPINFOHEADER);
		t_bitmapinfo . bmiHeader . biCompression = BI_RGB;
		t_bitmapinfo . bmiHeader . biWidth = t_clipped_bounds . width;
		t_bitmapinfo . bmiHeader . biHeight = -t_clipped_bounds . height;
		t_bitmapinfo . bmiHeader . biPlanes = 1;
		t_bitmapinfo . bmiHeader . biBitCount = 32;
		t_rgb_bitmap = CreateDIBSection(t_gdicontext, &t_bitmapinfo, DIB_RGB_COLORS, &t_rgb_data, NULL, 0);
		t_success = t_rgb_bitmap != NULL && t_rgb_data != NULL;
	}

	if (t_success)
		t_success = SelectObject(t_gdicontext, t_rgb_bitmap) != NULL;

	uint8_t *t_mask_pxls;
	uint32_t *t_rgb_pxls, *t_context_pxls;
	uint32_t t_context_width, t_x_offset, t_y_offset;
	if (t_success)
	{
		t_mask_pxls = (uint8_t *) t_mask_data;
		t_rgb_pxls = (uint32_t *) t_rgb_data;

		t_context_pxls = (uint32_t *) self -> layer-> canvas -> getTopDevice() -> accessBitmap(false) . getPixels();
		t_context_width = self -> layer-> canvas -> getTopDevice() -> accessBitmap(false) . width();	

		t_x_offset = t_device_location . x + t_clipped_bounds . x;
		t_y_offset = t_device_location . y + t_clipped_bounds . y;

		for(uint32_t y = 0; y < t_clipped_bounds . height; y++)
		{
			for(uint32_t x = 0; x < t_clipped_bounds . width; x++)
			{
				if (*(t_mask_pxls + y * t_clipped_bounds . width + x) != 0xFF)
				{
					uint32_t t_context_pxl;
					t_context_pxl = *(t_context_pxls + (y + t_y_offset) * t_context_width + x + t_x_offset);
					if (((t_context_pxl >> 24) & 0xFF) == 0x00)
						*(t_rgb_pxls + y * t_clipped_bounds . width + x) = t_context_pxl;
					else
						*(t_rgb_pxls + y * t_clipped_bounds . width + x) = 0x00FFFFFF;
				}
			}
		}

		SetBkMode(t_gdicontext, TRANSPARENT);
		SetTextColor(t_gdicontext, RGB((self -> state -> fill_color >> 16) & 0xFF, (self -> state -> fill_color >> 8) & 0xFF, (self -> state -> fill_color >> 0) & 0xFF));
		t_success = TextOutW(t_gdicontext, 0, 0, (LPCWSTR)p_text, p_length >> 1);
	}

	if (t_success)
		t_success = GdiFlush();

	if (t_success)
	{
		for(uint32_t y = 0; y < t_clipped_bounds . height; y++)
		{
			for(uint32_t x = 0; x < t_clipped_bounds . width; x++)
			{
				if (*(t_mask_pxls + y * t_clipped_bounds . width + x) != 0xFF)
				{
					uint32_t *t_context_pxl;
					t_context_pxl = t_context_pxls + (y + t_y_offset) * t_context_width + x + t_x_offset;
					if (((*t_context_pxl >> 24) & 0xFF) == 0x00)
						*t_context_pxl = *(t_rgb_pxls + y * t_clipped_bounds . width + x) | 0xFF000000;
					else
					{
						uint32_t t_lcd32_val;
						t_lcd32_val = *(t_rgb_pxls + y * t_clipped_bounds . width + x);		
						
						uint8_t t_a8_val;
						t_a8_val = 255 - ((t_lcd32_val & 0xFF) + ((t_lcd32_val & 0xFF00) >> 8) + ((t_lcd32_val & 0xFF0000) >> 16)) / 3;
						
						//*t_context_pxl = packed_bilinear_bounded(*t_context_pxl, 255 - t_a8_val, t_lcd32_val, t_a8_val);;
						*t_context_pxl = t_lcd32_val | (t_a8_val << 24);
					}
				}
			}
		}
	}

	DeleteObject(t_mask_bitmap);
	DeleteObject(t_rgb_bitmap);
	DeleteDC(t_gdicontext);
	self -> is_valid = t_success;
}
#endif

////////////////////////////////////////////////////////////////////////////////