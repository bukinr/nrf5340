/*-
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <dev/display/mipi_dcs.h>
#include <dev/gpio/gpio.h>

#include <libfont/libfont.h>

#include <arm/nordicsemi/nrf5340_app_core.h>

#include "oled.h"

static mdx_device_t gpio_dev;

extern struct nrf_gpio_softc gpio0_sc;

#define	PIN_MSCL	23
#define	PIN_MSDA	24

#define	PIN_CTRL	12
#define	PIN_DCX		11
#define	PIN_SCK		10
#define	PIN_MISO	9
#define	PIN_MOSI	8
#define	PIN_CSX		7

static struct global_data {
	struct font_info font;
} g_data;

static void
gpio_set_value(int pin, int val)
{

	mdx_gpio_set(gpio_dev, pin, val);
	///mdx_gpio_set(gpio_dev, pin, val);
}

static int
gpio_get_value(int pin)
{
	int val;

	val = mdx_gpio_get(gpio_dev, pin);
	///val = nrf_gpio_get(gpio_dev, pin);

	return (val);
}

static uint32_t
soft_spi_xfer_16bit_3wire(uint32_t dout)
{
	int spi_delay_us;
	uint32_t tmpdin;
	int bitlen;
	int j;

	spi_delay_us = 1;
	tmpdin = 0;
	bitlen = 8;

	//gpio_set_value(PIN_CSX, 0);
	//udelay(spi_cs_delay_us);
	for (j = bitlen - 1; j >= 0; j--) {
		gpio_set_value(PIN_SCK, 0);
		//printf("sending bit %d\n", (dout & (1 << j)) != 0);
		gpio_set_value(PIN_MOSI, (dout & (1 << j)) != 0);
		udelay(spi_delay_us);
		tmpdin = (tmpdin << 1) | gpio_get_value(PIN_MISO);
		gpio_set_value(PIN_SCK, 1);
		udelay(spi_delay_us);
	}
	//udelay(spi_cs_delay_us);
	//gpio_set_value(PIN_CSX, 1);
	//udelay(spi_cs_delay_us);

	return (tmpdin & 0xFFFF);
}

#if 1
static uint16_t
oled_read(uint8_t reg)
{
	int spi_cs_delay_us;
	int val;

	spi_cs_delay_us = 2;

	critical_enter();
	gpio_set_value(PIN_CSX, 0);
	gpio_set_value(PIN_DCX, 0);
	udelay(spi_cs_delay_us);

	soft_spi_xfer_16bit_3wire(reg);
	val = soft_spi_xfer_16bit_3wire(0);

	gpio_set_value(PIN_CSX, 1);
	gpio_set_value(PIN_DCX, 1);
	udelay(spi_cs_delay_us);
	critical_exit();

	return (val);
}
#endif

static void
oled_start_write(int reg)
{
	int spi_cs_delay_us;

	spi_cs_delay_us = 2;

	gpio_set_value(PIN_CSX, 0);
	gpio_set_value(PIN_DCX, 0);
	udelay(spi_cs_delay_us);

	soft_spi_xfer_16bit_3wire(reg);

	gpio_set_value(PIN_DCX, 1);
	udelay(spi_cs_delay_us);
}

static void
oled_stop_write(void)
{
	int spi_cs_delay_us;

	spi_cs_delay_us = 2;

	gpio_set_value(PIN_CSX, 1);
	udelay(spi_cs_delay_us);
}

static void
oled_write(uint8_t reg, int val)
{
	int spi_cs_delay_us;

	spi_cs_delay_us = 2;

	critical_enter();
	gpio_set_value(PIN_CSX, 0);
	gpio_set_value(PIN_DCX, 0);
	udelay(spi_cs_delay_us);

	soft_spi_xfer_16bit_3wire(reg);

	gpio_set_value(PIN_DCX, 1);
	udelay(spi_cs_delay_us);

	soft_spi_xfer_16bit_3wire(val);

	gpio_set_value(PIN_CSX, 1);
	udelay(spi_cs_delay_us);
	critical_exit();

	mdx_tsleep(1000);
}

static void
oled_write_many(int reg, int val1, int val2, int val3, int val4)
{
	int spi_cs_delay_us;

	spi_cs_delay_us = 2;

	critical_enter();
	gpio_set_value(PIN_CSX, 0);
	gpio_set_value(PIN_DCX, 0);
	udelay(spi_cs_delay_us);

	soft_spi_xfer_16bit_3wire(reg);

	gpio_set_value(PIN_DCX, 1);
	udelay(spi_cs_delay_us);

	soft_spi_xfer_16bit_3wire(val1);
	soft_spi_xfer_16bit_3wire(val2);
	soft_spi_xfer_16bit_3wire(val3);
	soft_spi_xfer_16bit_3wire(val4);

	gpio_set_value(PIN_CSX, 1);
	udelay(spi_cs_delay_us);
	critical_exit();

	mdx_tsleep(1000);
}

static void
oled_init(void)
{

	oled_write(0xFE, 0x04);
	oled_write(0x00, 0xdc);
	oled_write(0x01, 0x00);
	oled_write(0x02, 0x02);
	oled_write(0x03, 0x00);
	oled_write(0x04, 0x00);
	oled_write(0x05, 0x03);
	oled_write(0x06, 0x16);
	oled_write(0x07, 0x13);
	oled_write(0x08, 0x08);
	oled_write(0x09, 0xdc);
	oled_write(0x0a, 0x00);
	oled_write(0x0b, 0x02);
	oled_write(0x0c, 0x00);
	oled_write(0x0d, 0x00);
	oled_write(0x0e, 0x02);
	oled_write(0x0f, 0x16);
	oled_write(0x10, 0x18);
	oled_write(0x11, 0x08);
	oled_write(0x12, 0xC2);
	oled_write(0x13, 0x00);
	oled_write(0x14, 0x34);
	oled_write(0x15, 0x05);
	oled_write(0x16, 0x00);
	oled_write(0x17, 0x03);
	oled_write(0x18, 0x15);
	oled_write(0x19, 0x41);
	oled_write(0x1a, 0x00);
	oled_write(0x1b, 0xdc);
	oled_write(0x1c, 0x00);
	oled_write(0x1d, 0x04);
	oled_write(0x1e, 0x00);
	oled_write(0x1f, 0x00);
	oled_write(0x20, 0x03);
	oled_write(0x21, 0x16);
	oled_write(0x22, 0x18);
	oled_write(0x23, 0x08);
	oled_write(0x24, 0xdc);
	oled_write(0x25, 0x00);
	oled_write(0x26, 0x04);
	oled_write(0x27, 0x00);
	oled_write(0x28, 0x00);
	oled_write(0x29, 0x01);
	oled_write(0x2a, 0x16);
	oled_write(0x2b, 0x18);
	oled_write(0x2d, 0x08);
	oled_write(0x4c, 0x99);
	oled_write(0x4d, 0x00);
	oled_write(0x4e, 0x00);
	oled_write(0x4f, 0x00);
	oled_write(0x50, 0x01);
	oled_write(0x51, 0x0A);
	oled_write(0x52, 0x00);
	oled_write(0x5a, 0xe4);
	oled_write(0x5e, 0x77);
	oled_write(0x5f, 0x77);
	oled_write(0x60, 0x34);
	oled_write(0x61, 0x02);
	oled_write(0x62, 0x81);

	oled_write(0xFE, 0x07);
	oled_write(0x07, 0x4F);

	oled_write(0xFE, 0x01);
	oled_write(0x04, 0x80);
	oled_write(0x05, 0x65);
	oled_write(0x06, 0x1E);
	oled_write(0x0E, 0x8B);
	oled_write(0x0F, 0x8B);
	oled_write(0x10, 0x11);
	oled_write(0x11, 0xA2);
	oled_write(0x12, 0x80);
	oled_write(0x14, 0x81);
	oled_write(0x15, 0x82);
	oled_write(0x18, 0x45);
	oled_write(0x19, 0x34);
	oled_write(0x1A, 0x10);
	oled_write(0x1C, 0x57);
	oled_write(0x1D, 0x02);
	oled_write(0x21, 0xF8);
	oled_write(0x22, 0x90);
	oled_write(0x23, 0x00);
	oled_write(0x25, 0x0A);
	oled_write(0x26, 0x46);
	oled_write(0x2A, 0x47);
	oled_write(0x2B, 0xFF);
	oled_write(0x2D, 0xFF);
	oled_write(0x2F, 0xAE);
	oled_write(0x37, 0x0C);
	oled_write(0x3a, 0x00);
	oled_write(0x3b, 0x20);
	oled_write(0x3d, 0x0B);
	oled_write(0x3f, 0x38);
	oled_write(0x40, 0x0B);
	oled_write(0x41, 0x0B);
	oled_write(0x42, 0x11);
	oled_write(0x43, 0x44);
	oled_write(0x44, 0x22);
	oled_write(0x45, 0x55);
	oled_write(0x46, 0x33);
	oled_write(0x47, 0x66);
	oled_write(0x4c, 0x11);
	oled_write(0x4d, 0x44);
	oled_write(0x4e, 0x22);
	oled_write(0x4f, 0x55);
	oled_write(0x50, 0x33);
	oled_write(0x51, 0x66);
	oled_write(0x56, 0x11);
	oled_write(0x58, 0x44);
	oled_write(0x59, 0x22);
	oled_write(0x5a, 0x55);
	oled_write(0x5b, 0x33);
	oled_write(0x5c, 0x66);
	oled_write(0x61, 0x11);
	oled_write(0x62, 0x44);
	oled_write(0x63, 0x22);
	oled_write(0x64, 0x55);
	oled_write(0x65, 0x33);
	oled_write(0x66, 0x66);
	oled_write(0x6D, 0x90);
	oled_write(0x6E, 0x40);
	oled_write(0x70, 0xA5);
	oled_write(0x72, 0x04);
	oled_write(0x73, 0x15);

	oled_write(0xFE, 0x0A);
	oled_write(0x29, 0x90);

	oled_write(0xFE, 0x05);
	oled_write(0x05, 0x15);	/* TPS65631W: OVSS -2.4V */

	oled_write(0xFE, 0x00);
	oled_write(0x35, 0x00);
	oled_write(0x3A, 0x77);

	oled_write_many(0x2A, 0x00, 0x00, 0x00, 0xB3);
	oled_write_many(0x2B, 0x00, 0x00, 0x00, 0x77);

	oled_start_write(0x2A);
	soft_spi_xfer_16bit_3wire(0x00);
	soft_spi_xfer_16bit_3wire(0x00);
	soft_spi_xfer_16bit_3wire(0x00);
	soft_spi_xfer_16bit_3wire(0xB3);
	oled_stop_write();

	oled_write(0x53, 0x20);
	oled_write(0x11, 0x00);
	mdx_tsleep(120000);
	oled_write(0x29, 0x00);
}

static void
draw_pixel(void *arg, int x, int y, int pixel)
{

	if (pixel)
		pixel = 0xffffff;
	else
		pixel = 0;

	soft_spi_xfer_16bit_3wire((pixel >> 16) & 0xff);
	soft_spi_xfer_16bit_3wire((pixel >>  8) & 0xff);
	soft_spi_xfer_16bit_3wire((pixel >>  0) & 0xff);
}

static void
draw_text(char *str)
{
	struct char_info ci;
	uint8_t *buf;
	uint8_t *newptr;
	int i;
	int row, col;
	int c;

	buf = (uint8_t *)str;

	for (i = 0;; i++) {
		c = utf8_to_ucs2(buf, &newptr);
		if (c == -1)
			return;
		buf = newptr;

		get_char_info(&g_data.font, c, &ci);
		//printf("xs %d ys %d\n", ci.xsize, ci.ysize);

		row = i / 15;
		col = i % 15;

		/* col */
		oled_write_many(MIPI_DCS_SET_COLUMN_ADDRESS,
				0x00, (ci.xsize * col),
				0x00, (ci.xsize * (col + 1)) - 1);
		/* row */
		oled_write_many(MIPI_DCS_SET_PAGE_ADDRESS,
				0x00, (ci.ysize * row),
				0x00, (ci.ysize * (row + 1)) - 1);

		critical_enter();
		oled_start_write(0x2C);
		draw_char(&g_data.font, c);
		oled_stop_write();
		critical_exit();
	}
}

static void
oled_clear(void)
{
	int i;

	oled_write_many(0x2A, 0x00, 0x00, 0x00, 0xB3);
	oled_write_many(0x2B, 0x00, 0x00, 0x00, 0x77);

	critical_enter();
	oled_start_write(0x2C);
	for (i = 0; i < 120*180*3; i++)
		soft_spi_xfer_16bit_3wire(0);

	oled_stop_write();
	critical_exit();
}

int
oled_test(void)
{
	int reg;
	int val;

	gpio_dev = mdx_device_lookup_by_name("nrf_gpio", 0);
	if (gpio_dev == NULL)
		panic("could not find gpio device");

	reg = CNF_DIR_OUT | CNF_INPUT_DIS | CNF_PULL_DOWN;

	printf("%s\n", __func__);

	//nrf_gpio_pincfg(gpio_dev, PIN_MSCL, reg);
	//nrf_gpio_pincfg(gpio_dev, PIN_MSDA, CNF_DIR_OUT); //| CNF_PULL_UP);
	nrf_gpio_pincfg(gpio_dev, PIN_CTRL, reg);
	nrf_gpio_pincfg(gpio_dev, PIN_DCX, reg);
	nrf_gpio_pincfg(gpio_dev, PIN_CSX, reg);
	nrf_gpio_pincfg(gpio_dev, PIN_SCK, reg);
	nrf_gpio_pincfg(gpio_dev, PIN_MOSI, reg);
	nrf_gpio_pincfg(gpio_dev, PIN_MISO, CNF_PULL_DOWN);

	//mdx_gpio_configure(gpio_dev, PIN_MSCL, 1);
	//mdx_gpio_configure(gpio_dev, PIN_MSDA, 1);
	mdx_gpio_configure(gpio_dev, PIN_CTRL, MDX_GPIO_OUTPUT);
	mdx_gpio_configure(gpio_dev, PIN_DCX, MDX_GPIO_OUTPUT);
	mdx_gpio_configure(gpio_dev, PIN_CSX, MDX_GPIO_OUTPUT);
	mdx_gpio_configure(gpio_dev, PIN_SCK, MDX_GPIO_OUTPUT);
	mdx_gpio_configure(gpio_dev, PIN_MOSI, MDX_GPIO_OUTPUT);
	mdx_gpio_configure(gpio_dev, PIN_MISO, MDX_GPIO_INPUT);

	//mdx_gpio_set(gpio_dev, PIN_MSCL, 0);
	//mdx_gpio_set(gpio_dev, PIN_MSDA, 0);
	mdx_gpio_set(gpio_dev, PIN_CTRL, 0);
	mdx_gpio_set(gpio_dev, PIN_DCX, 1);
	mdx_gpio_set(gpio_dev, PIN_CSX, 1);
	mdx_gpio_set(gpio_dev, PIN_SCK, 0);
	mdx_gpio_set(gpio_dev, PIN_MOSI, 0);
	mdx_gpio_set(gpio_dev, PIN_MISO, 0);

	mdx_tsleep(200000);

	//gpio_set_value(PIN_MOSI, 1);
	//gpio_set_value(PIN_CSX, 1);
	//gpio_set_value(PIN_SCK, 1);

	int i;

	/* Set oled VSS voltage */
	for (i = 0; i < 21; i++) {
		mdx_gpio_set(gpio_dev, PIN_CTRL, 1);
		udelay(100);
		mdx_gpio_set(gpio_dev, PIN_CTRL, 0);
		udelay(100);
	}
	mdx_gpio_set(gpio_dev, PIN_CTRL, 1);

	font_init(&g_data.font, (uint8_t *)0x90000);
	g_data.font.draw_pixel = draw_pixel;
	g_data.font.draw_pixel_arg = &g_data;

	printf("initializing display...\n");
	oled_init();
	printf("display initialized\n");

	//oled_write(MIPI_DCS_SET_TEAR_OFF, 0);
	mdx_tsleep(200000);
	//oled_write(MIPI_DCS_WRITE_CONTROL_DISPLAY, (1 << 5));
	mdx_tsleep(200000);
	oled_write(MIPI_DCS_ENTER_IDLE_MODE, 0);
	mdx_tsleep(2000000);
	oled_write(MIPI_DCS_EXIT_IDLE_MODE, 0);
	mdx_tsleep(2000000);
	oled_write(MIPI_DCS_ENTER_IDLE_MODE, 0);

	oled_write_many(0x2A, 0x00, 0x00, 0x00, 0xB3);
	oled_write_many(0x2B, 0x00, 0x00, 0x00, 0x77);

	if (1 == 1) {
		//printf("miso %d\n", gpio_get_value(PIN_MISO));
		//val = oled_read(0x0c00);
		//val = oled_read(0x3a00);

		//oled_write(0x53, 0xff);
		//oled_write(0x51, 0xff);
		//oled_write(0x51, 0x80);
		//oled_write(0x51, 0);
		//oled_write(0x21, 0);
		//oled_write(0x23, 0);

		if (1 == 1) {
			printf("clear start\n");
			oled_clear();
			printf("clear end\n");
		}

		//oled_write_many(0x2A, 0x00, 0x00, 0x00, 0xB3);
		//oled_write_many(0x2B, 0x00, 0x00, 0x00, 0x77);
		//val = oled_read(0x2E);
		//val = oled_read(0x0d);
		//val = oled_read(0x04);
		val = oled_read(0x0C);
		printf("%s: val %x\n", __func__, val);
		val = oled_read(0x0C);
		printf("%s: val %x\n", __func__, val);
	}

	i = 0;
	if (1 == 1) {
		while (1) {
#if 0
			int z;
			if (z == 0) {
				gpio_set_value(PIN_MSCL, 1);
				z = 1;
			} else {
				gpio_set_value(PIN_MSCL, 0);
				z = 0;
			}
			printf("sda %d\n", gpio_get_value(PIN_MSDA));
#endif

			char text[128];
			//sprintf(text, "С Новым Годом! %d", i++);
			sprintf(text, "mdepx init %d", i);
			draw_text("mdepx started");
			i += 1;

			printf("sl\n");
			mdx_tsleep(1000000);
			printf("sl done\n");
		}
	}

	//mdx_tsleep(4000000);
	//printf("off\n");
	//oled_write(MIPI_DCS_SET_DISPLAY_OFF, 0);

	//mdx_tsleep(4000000);
	//printf("on\n");
	//oled_write(MIPI_DCS_SET_DISPLAY_ON, 0);

	//printf("deep standby\n");
	//oled_write(0x4f, 1);

	oled_write(MIPI_DCS_SET_TEAR_OFF, 0);
	oled_write(MIPI_DCS_WRITE_CONTROL_DISPLAY, (1 << 5));

	while (1) {
		mdx_tsleep(2000000);
		printf("enter idle\n");
		oled_write(MIPI_DCS_ENTER_IDLE_MODE, 0);

		mdx_tsleep(2000000);
		printf("exit idle\n");
		//oled_write(MIPI_DCS_EXIT_IDLE_MODE, 0);
	}

	while (1)
		mdx_tsleep(2000000);

	return (0);
}
