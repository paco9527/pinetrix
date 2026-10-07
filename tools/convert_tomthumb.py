#!/usr/bin/env python3
"""Convert TomThumb.h (Adafruit GFX format) to lvgl v8.3 font format."""

import re
import sys
import os

def parse_glyphs(content):
    """Parse TomThumbGlyphs array."""
    # Find start of the array
    start = content.find('TomThumbGlyphs[]')
    if start < 0:
        raise ValueError("Cannot find TomThumbGlyphs array")
    # Find the opening brace after `=`
    brace = content.find('{', start)
    if brace < 0:
        raise ValueError("Cannot find opening brace")
    # Find the closing `};` of the array
    end = content.find('};', brace)
    if end < 0:
        raise ValueError("Cannot find closing brace")
    
    body = content[brace:end]
    # Parse each {num, num, num, num, num, num} entry
    # \s* after { allows "{ 0," format. The array's own opening
    # brace ("{\n") won't match because \s* then hits "{" not a digit.
    glyphs = []
    for m in re.finditer(r'\{\s*(-?\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}', body):
        glyphs.append({
            'bitmap_ofs': int(m.group(1)),
            'w': int(m.group(2)),
            'h': int(m.group(3)),
            'x_adv': int(m.group(4)),
            'x_ofs': int(m.group(5)),
            'y_ofs': int(m.group(6)),
        })
    return glyphs

def parse_bitmap(content):
    """Parse TomThumbBitmaps array, return bytes."""
    start = content.find('TomThumbBitmaps[]')
    if start < 0:
        raise ValueError("Cannot find TomThumbBitmaps array")
    brace = content.find('{', start)
    if brace < 0:
        raise ValueError("Cannot find opening brace")
    end = content.find('};', brace)
    if end < 0:
        raise ValueError("Cannot find closing brace")

    body = content[brace:end]
    # Strip C comments -- comments like /*[N] 0xXX name */ contain hex
    # values that would be wrongly captured by the 0x hex regex.
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.DOTALL)
    data = []
    for m in re.finditer(r'0x([0-9A-Fa-f]{2})', body):
        data.append(int(m.group(1), 16))
    return bytes(data)

def get_char_range(glyphs):
    """Determine character range from glyph count.
    
    TomThumb.h defines TOMTHUMB_USE_EXTENDED=1 giving ~205 glyphs
    (0x20-0xEB). We only want basic ASCII 0x20-0x7E (95 glyphs).
    """
    first = 0x20
    last = 0x7E
    count = last - first + 1  # 95
    return first, last, count

def convert_glyph_bitmap(glyph, raw_bitmap):
    """Convert one glyph's bitmap to lvgl's tightly-packed bpp=1 format.

    TomThumb stores each row as a full byte (MSB = leftmost pixel),
    with only the high `w` bits used. lvgl packs bits tightly:
    row 0's `box_w` bits are followed immediately by row 1's bits,
    with no byte alignment between rows.

    So we need to extract the top `box_w` bits from each TomThumb row
    and pack them contiguously into the output buffer.
    """
    w = glyph['w']
    h = glyph['h']
    ofs = glyph['bitmap_ofs']

    if w == 0 or h == 0:
        return bytes()

    box_w = glyph['x_adv']  # effective pixel width used in lvgl
    total_bits = box_w * h
    total_bytes = (total_bits + 7) // 8
    out = bytearray(total_bytes)
    bit_pos = 0

    for row in range(h):
        row_byte = raw_bitmap[ofs + row]
        for col in range(box_w):
            # TomThumb: bit (7 - col) of row_byte is the pixel at (col, row)
            pixel = (row_byte >> (7 - col)) & 1
            if pixel:
                byte_idx = bit_pos // 8
                bit_idx = 7 - (bit_pos % 8)
                out[byte_idx] |= (1 << bit_idx)
            bit_pos += 1

    return bytes(out)

def generate_font_c(glyphs, raw_bitmap, output_path):
    """Generate lvgl font C source file."""
    first, last, count = get_char_range(glyphs)
    actual_glyphs = glyphs[:count]
    
    # Convert each glyph bitmap
    converted = []
    total_buf = b''
    new_bitmap_ofs = []
    
    for g in actual_glyphs:
        data = convert_glyph_bitmap(g, raw_bitmap)
        new_bitmap_ofs.append(len(total_buf))
        converted.append(data)
        total_buf += data
    
    # Calculate base line: TomThumb glyphs have ofs_y=-5, box_h=5,
    # so the glyph occupies [baseline-5, baseline]. Place baseline at 5
    # (line_height=6) so glyphs render in rows [0,5] -- visible on 8-row screen.
    line_height = 6
    base_line = 5
    
    # Font name
    font_name = "tomthumb"
    guarden = "FONT_TOMTHUMB"
    
    with open(output_path, 'w') as f:
        f.write(f'/*\n')
        f.write(f' * TomThumb 3x5 pixel font, converted to lvgl format.\n')
        f.write(f' * Original TomThumb license (3-clause BSD) applies.\n')
        f.write(f' */\n\n')
        f.write(f'#include "lvgl.h"\n\n')
        
        # Glyph descriptors
        # box_w: TomThumb stores each row as a full byte (w=8) but only the
        # high bits hold pixels (max effective width is 3px). xAdvance is the
        # real visual width. Using xAdvance as box_w crops the right padding.
        # ofs_y: lvgl computes glyph y as:
        #   gpos.y = label_y + (line_height - base_line) - box_h - ofs_y
        # To align glyph top with label top (gpos.y == label_y):
        #   ofs_y = (line_height - base_line) - box_h
        f.write(f'static const lv_font_fmt_txt_glyph_dsc_t {font_name}_glyph_dsc[] = {{\n')
        # lvgl 把 glyph_id 0 视为"未找到"(会画占位框), 故保留 id=0, 真实字形从 1 开始。
        f.write('    /* id = 0 reserved */\n')
        f.write('    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},\n')
        for i, g in enumerate(actual_glyphs):
            adv_w_scaled = g["x_adv"] * 16
            ofs_y = (line_height - base_line) - g["h"]
            f.write(f'    {{.bitmap_index = {new_bitmap_ofs[i]}, .adv_w = {adv_w_scaled}, '
                    f'.box_w = {g["x_adv"]}, .box_h = {g["h"]}, '
                    f'.ofs_x = {g["x_ofs"]}, .ofs_y = {ofs_y}}},\n')
        f.write(f'}};\n\n')
        
        # Bitmap data
        f.write(f'static const uint8_t {font_name}_bitmap[{len(total_buf)}] = {{\n')
        for i in range(0, len(total_buf), 16):
            row = total_buf[i:i+16]
            hex_vals = ', '.join(f'0x{b:02X}' for b in row)
            f.write(f'    {hex_vals},\n')
        f.write(f'}};\n\n')
        
        # CMAP: simple range, linear mapping
        f.write(f'static const lv_font_fmt_txt_cmap_t {font_name}_cmaps[] = {{\n')
        f.write(f'    {{\n')
        f.write(f'        .range_start = {first},\n')
        f.write(f'        .range_length = {count},\n')
        f.write(f'        .glyph_id_start = 1,\n')
        f.write(f'        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY,\n')
        f.write(f'        .unicode_list = NULL,\n')
        f.write(f'        .glyph_id_ofs_list = NULL,\n')
        f.write(f'    }},\n')
        f.write(f'}};\n\n')
        
        # Font descriptor
        f.write(f'static const lv_font_fmt_txt_dsc_t {font_name}_dsc = {{\n')
        f.write(f'    .glyph_bitmap = {font_name}_bitmap,\n')
        f.write(f'    .glyph_dsc = {font_name}_glyph_dsc,\n')
        f.write(f'    .cmaps = {font_name}_cmaps,\n')
        f.write(f'    .kern_dsc = NULL,\n')
        f.write(f'    .cmap_num = 1,\n')
        f.write(f'    .bpp = 1,\n')
        f.write(f'    .kern_scale = 16,\n')
        f.write(f'}};\n\n')
        
        # Font object
        f.write(f'const lv_font_t {font_name}_font = {{\n')
        f.write(f'    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n')
        f.write(f'    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n')
        f.write(f'    .line_height = {line_height},\n')
        f.write(f'    .base_line = {base_line},\n')
        f.write(f'    .subpx = LV_FONT_SUBPX_NONE,\n')
        f.write(f'    .underline_position = -1,\n')
        f.write(f'    .underline_thickness = 1,\n')
        f.write(f'    .dsc = (void*)&{font_name}_dsc,\n')
        f.write(f'    .fallback = NULL,\n')
        f.write(f'}};\n')
    
    print(f'Generated {output_path}: {count} glyphs, {len(total_buf)} bytes bitmap, range U+{first:04X}-U+{last:04X}')

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    
    input_path = os.path.join(repo_root, 'core', 'modules', 'graph', 'TomThumb.h')
    output_path = os.path.join(repo_root, 'core', 'display', 'font_tomthumb.c')
    
    print(f'Reading {input_path}')
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    glyphs = parse_glyphs(content)
    print(f'Parsed {len(glyphs)} glyphs')
    
    raw_bitmap = parse_bitmap(content)
    print(f'Parsed {len(raw_bitmap)} bytes bitmap data')
    
    generate_font_c(glyphs, raw_bitmap, output_path)

if __name__ == '__main__':
    main()
