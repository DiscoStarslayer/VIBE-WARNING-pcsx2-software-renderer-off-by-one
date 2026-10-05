// SPDX-License-Identifier: MIT
// (AI-assisted) PS2 GS texture-size boundary reproducer; no game assets.

#include <dma.h>
#include <draw.h>
#include <gif_tags.h>
#include <graph.h>
#include <gs_gp.h>
#include <gs_psm.h>
#include <kernel.h>
#include <stdio.h>
#include <tamtypes.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448
#define TEXTURE_SIDE 64
#define SELECTED_ROW 32
#define PACKET_QWORDS 4096

static qword_t commands[PACKET_QWORDS] __attribute__((aligned(128)));
static u32 texture[TEXTURE_SIDE * TEXTURE_SIDE] __attribute__((aligned(128)));
static int command_count;
static int texture_address;

// Small geometric lettering keeps labels independent of texture sampling.
static const unsigned char letters[26][7] = {
	{14, 17, 17, 31, 17, 17, 17}, // A
	{30, 17, 17, 30, 17, 17, 30}, // B
	{14, 17, 16, 16, 16, 17, 14}, // C
	{30, 17, 17, 17, 17, 17, 30}, // D
	{31, 16, 16, 30, 16, 16, 31}, // E
	{31, 16, 16, 30, 16, 16, 16}, // F
	{14, 17, 16, 23, 17, 17, 14}, // G
	{17, 17, 17, 31, 17, 17, 17}, // H
	{14, 4, 4, 4, 4, 4, 14}, // I
	{7, 2, 2, 2, 2, 18, 12}, // J
	{17, 18, 20, 24, 20, 18, 17}, // K
	{16, 16, 16, 16, 16, 16, 31}, // L
	{17, 27, 21, 21, 17, 17, 17}, // M
	{17, 25, 21, 19, 17, 17, 17}, // N
	{14, 17, 17, 17, 17, 17, 14}, // O
	{30, 17, 17, 30, 16, 16, 16}, // P
	{14, 17, 17, 17, 21, 18, 13}, // Q
	{30, 17, 17, 30, 20, 18, 17}, // R
	{15, 16, 16, 14, 1, 1, 30}, // S
	{31, 4, 4, 4, 4, 4, 4}, // T
	{17, 17, 17, 17, 17, 17, 14}, // U
	{17, 17, 17, 17, 17, 10, 4}, // V
	{17, 17, 17, 21, 21, 21, 10}, // W
	{17, 17, 10, 4, 10, 17, 17}, // X
	{17, 17, 10, 4, 4, 4, 4}, // Y
	{31, 1, 2, 4, 8, 16, 31}, // Z
};

static const unsigned char digits[10][7] = {
	{14, 17, 19, 21, 25, 17, 14},
	{4, 12, 4, 4, 4, 4, 14},
	{14, 17, 1, 2, 4, 8, 31},
	{30, 1, 1, 14, 1, 1, 30},
	{2, 6, 10, 18, 31, 2, 2},
	{31, 16, 16, 30, 1, 1, 30},
	{14, 16, 16, 30, 17, 17, 14},
	{31, 1, 2, 4, 8, 8, 8},
	{14, 17, 17, 14, 17, 17, 14},
	{14, 17, 17, 15, 1, 1, 14},
};

static void flush_commands(void)
{
	if (command_count == 0)
		return;

	qword_t* start = commands;
	PACK_GIFTAG(start, GIF_SET_TAG(command_count, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
	qword_t* end = draw_finish(&commands[command_count + 1]);
	dma_channel_send_normal(DMA_CHANNEL_GIF, commands, end - commands, 0, 0);
	dma_wait_fast();
	draw_wait_finish();
	command_count = 0;
}

static void reg_write(int reg, u64 value)
{
	if (command_count >= PACKET_QWORDS - 4)
		flush_commands();
	++command_count;
	qword_t* command = &commands[command_count];
	PACK_GIFTAG(command, value, reg);
}

static void solid_rect(int x0, int y0, int x1, int y1, u32 color)
{
	reg_write(GS_REG_PRIM, GIF_SET_PRIM(PRIM_SPRITE, 0, 0, 0, 0, 0, 1, 0, 0));
	reg_write(GS_REG_RGBAQ, (u64)color | ((u64)0x3f800000 << 32));
	reg_write(GS_REG_XYZ2, GIF_SET_XYZ(x0 << 4, y0 << 4, 0));
	reg_write(GS_REG_XYZ2, GIF_SET_XYZ(x1 << 4, y1 << 4, 0));
}

static void text(int x, int y, int scale, const char* message)
{
	for (; *message; ++message, x += 6 * scale)
	{
		const unsigned char* glyph = NULL;
		if (*message >= 'A' && *message <= 'Z')
			glyph = letters[*message - 'A'];
		else if (*message >= '0' && *message <= '9')
			glyph = digits[*message - '0'];
		if (!glyph)
			continue;
		for (int row = 0; row < 7; ++row)
			for (int column = 0; column < 5; ++column)
				if (glyph[row] & (1 << (4 - column)))
					solid_rect(x + column * scale, y + row * scale,
						x + (column + 1) * scale, y + (row + 1) * scale, 0x80eeeeee);
	}
}

static void texture_vertex(int x, int y, int u, int v)
{
	reg_write(GS_REG_UV, GIF_SET_UV((u << 4) + 8, (v << 4) + 8));
	reg_write(GS_REG_XYZ2, GIF_SET_XYZ(x << 4, y << 4, 0));
}

static void panel(int left, int tw, int th)
{
	solid_rect(left - 2, 140, left + 178, 342, 0x80eeeeee);
	reg_write(GS_REG_TEX0_1, GS_SET_TEX0(texture_address >> 6, 1, GS_PSM_32,
		tw, th, TEXTURE_COMPONENTS_RGBA, TEXTURE_FUNCTION_DECAL, 0, 0, 0, 0, 0));
	reg_write(GS_REG_TEX1_1, GS_SET_TEX1(1, 0, 0, 0, 0, 0, 0));
	// All V coordinates select row 32; U selects columns 0..63.
	reg_write(GS_REG_CLAMP_1, GS_SET_CLAMP(WRAP_REGION_REPEAT, WRAP_REGION_REPEAT,
		TEXTURE_SIDE - 1, 0, 0, SELECTED_ROW));
	reg_write(GS_REG_PRIM, GIF_SET_PRIM(PRIM_TRIANGLE_STRIP, 0, 1, 0, 0, 0, 1, 0, 0));
	reg_write(GS_REG_RGBAQ, GIF_SET_RGBAQ(128, 128, 128, 128, 0x3f800000));
	texture_vertex(left, 142, 0, 0);
	texture_vertex(left, 340, 0, 63);
	texture_vertex(left + 176, 142, 63, 0);
	texture_vertex(left + 176, 340, 63, 63);
}

static void init_gs(void)
{
	const int framebuffer_address = graph_vram_allocate(SCREEN_WIDTH, SCREEN_HEIGHT,
		GS_PSM_32, GRAPH_ALIGN_PAGE);
	texture_address = graph_vram_allocate(TEXTURE_SIDE, TEXTURE_SIDE, GS_PSM_32, GRAPH_ALIGN_BLOCK);
	graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
	graph_set_screen(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	graph_set_bgcolor(0, 0, 0);
	graph_set_framebuffer_filtered(framebuffer_address, SCREEN_WIDTH, GS_PSM_32, 0, 0);
	graph_enable_output();

	reg_write(GS_REG_FRAME_1, GS_SET_FRAME(framebuffer_address >> 11, SCREEN_WIDTH >> 6, GS_PSM_32, 0));
	reg_write(GS_REG_ZBUF_1, GS_SET_ZBUF(0, 0, 1));
	reg_write(GS_REG_XYOFFSET_1, GS_SET_XYOFFSET(0, 0));
	reg_write(GS_REG_SCISSOR_1, GS_SET_SCISSOR(0, SCREEN_WIDTH - 1, 0, SCREEN_HEIGHT - 1));
	reg_write(GS_REG_TEST_1, GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
	reg_write(GS_REG_PRMODECONT, GS_SET_PRMODECONT(1));
	reg_write(GS_REG_COLCLAMP, GS_SET_COLCLAMP(1));
	reg_write(GS_REG_DTHE, 0);
	reg_write(GS_REG_PABE, 0);
	reg_write(GS_REG_FBA_1, 0);
	flush_commands();

	for (int y = 0; y < TEXTURE_SIDE; ++y)
		for (int x = 0; x < TEXTURE_SIDE; ++x)
			texture[y * TEXTURE_SIDE + x] = y == SELECTED_ROW ?
				((x / 8) & 1 ? 0x8070e090 : 0x8030b050) : 0x80b040b0;

	qword_t* end = draw_texture_transfer(commands, texture, TEXTURE_SIDE, TEXTURE_SIDE,
		GS_PSM_32, texture_address, TEXTURE_SIDE);
	end = draw_texture_flush(end);
	FlushCache(0); // The DMA chain references the separate texture buffer.
	dma_channel_send_chain(DMA_CHANNEL_GIF, commands, end - commands, 0, 0);
	dma_wait_fast();
}

int main(void)
{
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	init_gs();
	printf("(AI-assisted) Software texture-cache boundary reproducer.\n");
	printf("(AI-assisted) All three panels must show identical green stripes.\n");
	printf("(AI-assisted) CONTROL: TW=6 TH=6; REDUCE: TW=10 TH=10; EXTEND: TW=6 TH=5.\n");
	printf("(AI-assisted) All use REGION_REPEAT with MINV=0 MAXV=32.\n");

	for (;;)
	{
		graph_wait_vsync();
		solid_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x80202020);
		text(158, 28, 2, "GS TEXTURE SIZE BOUNDARY");
		text(164, 58, 2, "REGION REPEAT SELECTS ROW 32");
		text(76, 100, 2, "CONTROL");
		text(284, 100, 2, "REDUCE");
		text(480, 100, 2, "EXTEND");
		text(80, 122, 1, "64 X 64");
		text(282, 122, 1, "1024 X 1024");
		text(482, 122, 1, "64 X 32");
		panel(32, 6, 6);
		panel(232, 10, 10);
		panel(432, 6, 5);
		text(158, 370, 2, "ALL THREE PANELS MUST MATCH");
		text(260, 408, 1, "AI ASSISTED REPRO");
		flush_commands();
	}
}
