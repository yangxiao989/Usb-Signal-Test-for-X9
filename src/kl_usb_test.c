/*
 * xusb: Generic USB test program
 * Copyright © 2009-2012 Pete Batard <pete@akeo.ie>
 * Contributions to Mass Storage by Alan Stern.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "libusb.h"

#define USB_PHY_NCR_CTRL0	0x10000
#define USB_PHY_NCR_CTRL1	0x10004
#define USB_PHY_NCR_CTRL2	0x10008
#define USB_PHY_NCR_CTRL3	0x1000c
#define USB_PHY_NCR_CTRL4	0x10010
#define USB_PHY_NCR_CTRL5	0x10014
#define USB_PHY_NCR_CTRL6	0x10018
#define USB_PHY_NCR_CTRL7	0x1001c

#define USB_CTRL_NCR_INTE	0xD000
#define USB_CTRL_NCR_CTRL0	0xD010
#define USB_CTRL_NCR_CTRL1	0xD014
#define USB_CTRL_NCR_CTRL2	0xD018
#define USB_CTRL_NCR_CTRL3	0xD01c
#define USB_CTRL_NCR_CTRL4	0xD020
#define USB_CTRL_NCR_CTRL5	0xD024
#define USB_CTRL_NCR_CTRL6	0xD028
#define USB_CTRL_NCR_CTRL7	0xD02c

#define PHY_NCR_REG		0x70026A33
#define PHY_NCR_REG_MASK	0x00020233

#define USB_TEST_J 1
#define USB_TEST_K 2
#define USB_TEST_SE0 3
#define USB_TEST_PACKET 4
#define USB_TEST_SOF 5

/* low level macros for accessing memory mapped hardware registers */
#define REG64(addr) ((volatile uint64_t *)(uintptr_t)(addr))
#define REG32(addr) ((volatile uint32_t *)(uintptr_t)(addr))
#define REG16(addr) ((volatile uint16_t *)(uintptr_t)(addr))
#define REG8(addr) ((volatile uint8_t *)(uintptr_t)(addr))

#define RMWREG64(addr, startbit, width, val) *REG64(addr) = (*REG64(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))
#define RMWREG32(addr, startbit, width, val) *REG32(addr) = (*REG32(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))
#define RMWREG16(addr, startbit, width, val) *REG16(addr) = (*REG16(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))
#define RMWREG8(addr, startbit, width, val) *REG8(addr) = (*REG8(addr) & ~(((1<<(width)) - 1) << (startbit))) | ((val) << (startbit))

#define writeq(v, a) (*REG64(a) = (v))
#define readq(a) (*REG64(a))
#define writel(v, a) (*REG32(a) = (v))
#define readl(a) (*REG32(a))
#define writeb(v, a) (*REG8(a) = (v))
#define readb(a) (*REG8(a))

//#define USB_SET_CLK_CMD     _IOWR('D', 8, struct usb_data)

static bool host_test_mode = false;
static bool device_test_mode = false;
static bool hub_test_mode = false;
static bool upstream_flag = false;

static void perr(char const *format, ...)
{
	va_list args;

	va_start (args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

static uint16_t VID, PID, PORTNUM;

struct usb_data {
	int port;
	int internal;
	int enable;
};

/*int enable_clk(int port, int internal, int enable)
{
	int fd, ret;
	struct usb_data data;

	data.port = port;
	data.internal = internal;
	data.enable = enable;

	fd = open("/dev/usbclk", O_RDWR);
	if (fd < 0) {
		printf("open /dev/usbclk fail fd %d\n", fd);
		return fd;
	}

	ret = ioctl(fd, USB_SET_CLK_CMD, &data);
	if (ret < 0) {
		printf("ioctl fail\n");
		return ret;
	}

	close(fd);
}*/

void usb_check_link_state (void *usbctrlcr_base) {
	unsigned int gctl = readl(usbctrlcr_base + 0xC110);
	unsigned int gctl_opmode = (gctl>>12) & 0x00000003;
	unsigned int portsc_u2 = readl(usbctrlcr_base + 0x420);
	unsigned int portsc_u3 = readl(usbctrlcr_base + 0x430);
	unsigned int portsc_u2_spd = (portsc_u2>>10) & 0x0000000f;
	unsigned int portsc_u3_spd = (portsc_u3>>10) & 0x0000000f;
	unsigned int portsc_u3lt = (portsc_u3>>5) & 0x0000000f;
	unsigned int ltssm = readl(usbctrlcr_base + 0xC164);
	unsigned int ltssm_linkstate = (ltssm>>22) & 0x0000000F;
	unsigned int ltssm_substate = (ltssm>>18) & 0x0000000F;
	unsigned int dsts = readl(usbctrlcr_base + 0xC70C);
	unsigned int dsts_speed = dsts & 0x00000007;
	unsigned int dsts_linkstate = (dsts>>18) & 0x0000000f;
	gctl_opmode = (gctl>>12) & 0x00000003;

	if (gctl_opmode==0x2) { // Device mode
	    printf("DSTS: %08X, DSTS_SPEED: %X, DSTS_LINK: %X.\n",dsts,dsts_speed,dsts_linkstate);
	    printf("LTSSM: %08X, LTSSM_LINK: %08X, LTSSM_SUB: %0X.\n",ltssm,ltssm_linkstate,ltssm_substate);
	} else {
	    printf("PORTSC_U2: %08X, PORTSC_U3: %08X, PORTSC_U3_Link: %0X, PORTSC_U2_SPD %0X, PORTSC_U3_SPD %X.\n",portsc_u2,portsc_u3,portsc_u3lt, portsc_u2_spd, portsc_u3_spd);
	    printf("LTSSM: %08X, LTSSM_LINK: %08X, LTSSM_SUB: %0X.\n",ltssm,ltssm_linkstate,ltssm_substate);
	}
}

static int usb_init(int phy_num, void *base, int usb_mode, int usb_speed, int regs1, int regs2, int regs3)
{
	void *phybase = base+0x20000;
	unsigned int data;
	int i;

	/* use internal phy clock and reset usb phy, reset high effective */
	data = readl(phybase + USB_PHY_NCR_CTRL0);
	data &= ~(1<<18);
	data |= (1<<0);
	writel(data, phybase + USB_PHY_NCR_CTRL0);

	if (phy_num == 1) {
//		enable_clk(1, 0, 0);
//		enable_clk(1, 1, 1);
		printf("\033[31musb phy 1 internal clk\033[00m\n");
		writel(0x41000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x69254000, phybase + USB_PHY_NCR_CTRL1);
		writel(0x0E2C7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x3E700800, phybase + USB_PHY_NCR_CTRL3);
		writel(PHY_NCR_REG_MASK | (regs1<<6 | regs2<<11 | regs3<<13), phybase + USB_PHY_NCR_CTRL4);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL5);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL6);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL7);
	} else if (phy_num == 2) {
//		enable_clk(2, 0, 0);
//		enable_clk(2, 1, 1);
		printf("\033[31musb phy 2 internal clk\033[00m\n");
		writel(0x41000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x69254000, phybase + USB_PHY_NCR_CTRL1);
		writel(0x0E2C7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x3E700800, phybase + USB_PHY_NCR_CTRL3);
		writel(PHY_NCR_REG_MASK | (regs1<<6 | regs2<<11 | regs3<<13), phybase + USB_PHY_NCR_CTRL4);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL5);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL6);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL7);
	}

	if (1) {
		printf("\033[31musb init ctrl ncr\033[00m\n");
		writel(0x00000003, base + USB_CTRL_NCR_INTE);
		writel(0x00210080, base + USB_CTRL_NCR_CTRL0);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL1);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL2);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL3);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL4);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL5);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL6);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL7);
		
		// GUSB3PIPECTL
		writel(0x010C0002, base + 0xC2C0);
	}
	usleep(30);

	// phy reset
	data = readl(phybase + USB_PHY_NCR_CTRL0);
	data &= ~(1<<0);
	writel(data, phybase + USB_PHY_NCR_CTRL0);

	if (usb_mode == 1) { //Device
		printf("\033[31musb set Device mode\033[00m\n");
		// set Device mode
		data = readl(base + 0xc110);
		data &= ~(0x3<<12);
		data |= (0x2<<12);
		writel(data, base + 0xC110);

		// set speed
		data = readl(base + 0xc700);
		data &= ~(0x7);
		if (usb_speed == 1) {
			data |= 1; //full
		} else if (usb_speed == 2) {
			data |= 0; // high
		} else if (usb_speed == 3) {
			data |= 4; // super
		}
		writel(data, base + 0xC700);

		// set bit 30
		data = readl(base + 0xc704);
		data |= (0x1<<30);
		writel(data, base + 0xC704);
		usleep(5);
		i = 1000;
		while (i--)
			if ((readl(base + 0xc704) & (1<<30)) == 0)
				break;
		if (i==0)
			printf("read 0x24 timeout\n");

		usleep(50);
	} else if (usb_mode == 2) { // Host
		printf("\033[31musb set Host mode\033[00m\n");
		// set host mode
		data = readl(base + 0xc110);
		data &= ~(0x3<<12);
		data |= (0x1<<12);
		writel(data, base + 0xC110);
		i = 1000;
		while (i--)
			if ((readl(base + 0x24) & (1<<11)) == 0)
				break;
		if (i==0)
			printf("read 0x24 timeout\n");

		data = readl(base + 0xc110);
		data |= (0x1<<11);
		writel(data, base + 0xC110);
		i = 1000;
		while (i--)
			if ((readl(base + 0xc110) & (1<<11)) == 0)
				break;
		if (i==0)
			printf("read 0xc110 timeout\n");

		usleep(500);
	
		data = readl(base + 0xc110);
		data &= ~(0x1<<11);
		writel(data, base + 0xC110);
	
		writel(0x2a0, base + 0x420);
		writel(0x2a0, base + 0x430);
	}

	printf("init ok\n");
	return 0;
}

static int test_device(uint16_t vid, uint16_t pid, uint16_t portnum)
{
	libusb_device_handle *handle;
	int i, r;
	uint8_t hub_desc[100];
	int maxchild = 0;


	printf("Opening device %04X:%04X...\n", vid, pid);
	handle = libusb_open_device_with_vid_pid(NULL, vid, pid);

	if (handle == NULL) {
		perr("  Failed.\n");
		return -1;
	}

	if (hub_test_mode) {
		r = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR, LIBUSB_DT_HUB<<8, 0, hub_desc, sizeof(hub_desc), 1000);
		if (r<0) {
			printf("read failed\n");
			return -1;
		} else {
			maxchild = hub_desc[2];
			printf("hub maxchild : %d\n", maxchild);
		}

		if (upstream_flag) {
			printf("Test upstream\n");
			libusb_control_transfer(handle, 0x00,
				0x03, 0x0002, 0x0400, 0, 0, 100);
		} else if (portnum > maxchild) {
			printf("Please type a num smaller than maxchild %d!\n", maxchild);
			return -1;
		} else {
			printf("Test downstream port %d\n", portnum);
			for (i = 1; i < (maxchild+1); i++) {
				if (i == portnum)
					continue;
				else
					libusb_control_transfer(handle, 0x23,
							0x03, 0x0002, 0x00<<8 | i, 0, 0, 100);
			}
			libusb_control_transfer(handle, 0x23,
					0x03, 0x0015, 0x04<<8 | portnum, 0, 0, 100);
		}
	}

	printf("Closing device...\n");
	libusb_close(handle);

	return 0;
}

int main(int argc, char** argv)
{
	bool show_help = false;
	int j, r, fd;
	size_t i, arglen;
	unsigned tmp_vid, tmp_pid, tmp_portnum;
	unsigned int usb_mode, usb_num, usb_speed, test_pattern, super_flag, regs1, regs2, regs3, addr;
	void *base;

	// Default to generic, expecting VID:PID
	VID = 0;
	PID = 0;
	PORTNUM = 0;

	if (argc >= 2) {
		for (j = 1; j<argc; j++) {
			arglen = strlen(argv[j]);
			if ( (argv[j][0] == '-') && (arglen >= 2) ) {
				if (strcmp(argv[j], "-host") == 0) {
					if (argc < 7) {
						printf("Please provide more parameters\n");
						return 1;
					}
					usb_mode = 2;
					host_test_mode = true;
				} else if (strcmp(argv[j], "-device") == 0) {
					if (argc < 7) {
						printf("Please provide more parameters\n");
						return 1;
					}
					usb_mode = 1;
					device_test_mode = true;
				} else if ((argv[j][1] == 'h') && (argv[j][2] == 'u') && (argv[j][3] == 'b')) {
					if ((arglen <= 4) || argv[j][4] != '=') {
						printf("Please specify port number to be test as \"-hub=portnum\" in decimal format\n");
						return 1;
					} else {
						if (sscanf(argv[j], "-hub=%d", &tmp_portnum) != 1) {
							printf("Please specify port number(num >= 0) to be test as \"-hub=portnum\" in decimal format\n");
							return 1;
						}
						hub_test_mode = true;
						if ((PORTNUM = (uint16_t)tmp_portnum) == 0) {
							upstream_flag = true;
							printf("portnum %d\n", PORTNUM);
						}
					}
				} else if (strcmp(argv[j], "-help") == 0) {
					show_help = true;
				}
			} else {
				for (i=0; i<arglen; i++) {
					if (argv[j][i] == ':')
						break;
				}
				if (i != arglen) {
					if (sscanf(argv[j], "%x:%x" , &tmp_vid, &tmp_pid) != 2) {
						printf("   Please specify VID & PID as \"vid:pid\" in hexadecimal format\n");
						return 1;
					}
					VID = (uint16_t)tmp_vid;
					PID = (uint16_t)tmp_pid;
				}
			}
		}
	}

	if ((show_help) || (argc == 1)) {
		printf("usage: %s [help] [-hub=num vid:pid] [-host] [-device]\n", argv[0]);
		printf("   help        : display usage\n");
		printf("   -host       : host_test_mode\n");
		printf("   -device     : device_test_mode\n");
		printf("	usb_num, usb_speed, test_mode, ncr_phy_regs are necessary under host and device test\n");
		printf("	[usb_num]   1: usb1 2: usb2\n");
		printf("	[usb_speed] 1: full 2: high, 3: super, 0: low\n");
		printf("	[test_mode] mode1~mode5\n");
		printf("	[ncr_phy_regs] \n");
		printf("   -hub=num    : hub_test_mode [num = 0 : upstream] [num >= 1 : specify downstream port to be test]\n");
		printf("	[vid:pid] is necessary under hub_test_mode\n");
		return 0;
	}

	if (hub_test_mode && !host_test_mode && !device_test_mode) {
		r = libusb_init(NULL);
		if (r < 0)
			return r;

		test_device(VID, PID, PORTNUM);

		libusb_exit(NULL);

		return 0;
	}

	if (!hub_test_mode && (host_test_mode || device_test_mode)) {
		super_flag = 0;
		usb_num = atoi(argv[2]);
		usb_speed = atoi(argv[3]);
		test_pattern = atoi(argv[4]);
		regs1 = atoi(argv[5]);
		regs2 = atoi(argv[6]);
		regs3 = atoi(argv[7]);
		if (argc > 8)
			super_flag = atoi(argv[8]);

		printf("enter test %d %d %d \nTUNE: 0x%x\n", regs1, regs2, regs3, PHY_NCR_REG_MASK | (regs1<<6 | regs2<<11 | regs3<<13));
		if (usb_num == 1)
			addr = 0x31220000;
		else
			addr = 0x31260000;
		printf("usb %d mode %d speed %d addr %lx test_pattern %d\n", usb_num, usb_mode, usb_speed, addr, test_pattern);

		fd = open("/dev/mem", O_RDWR);
		if (fd < 0) {
			printf("open /dev/mem fail fd %d\n", fd);
			return fd;
		}

		base = mmap(NULL, 0x40000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
		if (base == NULL) {
			printf("map  fail\n");
			return -1;
		}

		if (super_flag) {
			RMWREG32(base+0xc2c0, 30, 1, 0);
			RMWREG32(base+0xc2c0, 30, 1, 1);
			printf("super speed test pattern +1\n");
			return 0;
		}

		usb_init(usb_num, base, usb_mode, usb_speed, regs1, regs2, regs3);
		sleep(1);
		printf("usb init ok\n");
	
		if (usb_mode == 1) { //devices
			if (usb_speed == 1) { //full
				RMWREG32(base+0xc704, 1, 4, test_pattern);
				RMWREG32(base+0xc704, 31, 1, 1);
				usb_check_link_state(base);
			} else if (usb_speed == 2) { //high
				RMWREG32(base+0xc704, 1, 4, test_pattern);
				RMWREG32(base+0xc704, 31, 1, 1);
				usb_check_link_state(base);
			} if (usb_speed == 3) { //super
				RMWREG32(base+0xc2c0, 30, 1, 1);
				RMWREG32(base+0xc704, 31, 1, 1);
				usb_check_link_state(base);
			}
		} else if (usb_mode == 2) { //host
			if (usb_speed == 1) { //low
				RMWREG32(base+0x424, 28, 4, test_pattern);
				if (test_pattern == USB_TEST_SOF)
					RMWREG32(base+0x20, 0, 1, 1);
			} else if (usb_speed == 1) { //full
				RMWREG32(base+0x424, 28, 4, test_pattern);
				if (test_pattern == USB_TEST_SOF)
					RMWREG32(base+0x20, 0, 1, 1);
			} else if (usb_speed == 2) { //high
				RMWREG32(base+0x424, 28, 4, test_pattern);
				if (test_pattern == USB_TEST_SOF)
					RMWREG32(base+0x20, 0, 1, 1);
			} if (usb_speed == 3) { //super
				RMWREG32(base+0x430, 9, 1, 0);
				RMWREG32(base+0xc2c0, 30, 1, 1);
				RMWREG32(base+0x430, 9, 1, 1);
			}
		}
		while (1) {
			usleep(100);	
			usb_check_link_state(base);
		}
		return 0;
	}
}
