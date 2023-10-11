#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> 
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define USB_PHY_NCR_CTRL0	0x10000
#define USB_PHY_NCR_CTRL1	0x10004
#define USB_PHY_NCR_CTRL2	0x10008
#define USB_PHY_NCR_CTRL3	0x1000c
#define USB_PHY_NCR_CTRL4	0x10010
#define USB_PHY_NCR_CTRL5	0x10014
#define USB_PHY_NCR_CTRL6	0x10018
#define USB_PHY_NCR_CTRL7	0x1001c

#define USB_CTRL_NCR_INTE	0xE000
#define USB_CTRL_NCR_CTRL0	0xE010
#define USB_CTRL_NCR_CTRL1	0xE014
#define USB_CTRL_NCR_CTRL2	0xE018
#define USB_CTRL_NCR_CTRL3	0xE01c
#define USB_CTRL_NCR_CTRL4	0xE020
#define USB_CTRL_NCR_CTRL5	0xE024
#define USB_CTRL_NCR_CTRL6	0xE028
#define USB_CTRL_NCR_CTRL7	0xE02c

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

#define USB_SET_CLK_CMD     _IOWR('D', 8, struct usb_data)

struct usb_data {
	int port;
	int internal;
	int enable;
};

int enable_clk(int port, int internal, int enable)
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
}


static int usb_init(int phy_num, void *base, int usb_mode, int usb_speed, int clk_select)
{
	void *phybase = base+0x20000;
	unsigned int data;
	int i;

	/* use internal phy clock and reset usb phy, reset high effective */
	data = readl(phybase + USB_PHY_NCR_CTRL0);
	data &= ~(1<<18);
	data |= (1<<0);
	writel(data, phybase + USB_PHY_NCR_CTRL0);

	if (phy_num == 1 && clk_select ==2) {
		printf("\033[31musb phy 1 external clk\033[00m\n");
		enable_clk(1, 0, 1);
		enable_clk(1, 1, 0);
		//writel(0x40140005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x41140005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x6924E000, phybase + USB_PHY_NCR_CTRL1);
		//writel(0x0BAC7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x0E2C7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x3E700800, phybase + USB_PHY_NCR_CTRL3);
		writel(0x70026A33, phybase + USB_PHY_NCR_CTRL4);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL5);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL6);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL7);
	} else if (phy_num == 1 && clk_select == 1) {
		//enable_clk(1, 0, 0);
		//enable_clk(1, 1, 1);
		printf("\033[31musb phy 1 internal clk\033[00m\n");
		//writel(0x40000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x41000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x69254000, phybase + USB_PHY_NCR_CTRL1);
		//writel(0x0BAC7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x0E2C7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x3E700800, phybase + USB_PHY_NCR_CTRL3);
		writel(0x70026A33, phybase + USB_PHY_NCR_CTRL4);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL5);
		writel(0x00000001, phybase + USB_PHY_NCR_CTRL6);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL7);
	} else if (phy_num == 2 &&  clk_select == 2) {
		enable_clk(2, 0, 1);
		enable_clk(2, 1, 0);
		printf("\033[31musb phy 2 external clk\033[00m\n");
		//writel(0x40000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x41000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x6924E000, phybase + USB_PHY_NCR_CTRL1);
		//writel(0x0BAC7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x0E2C7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x3E700800, phybase + USB_PHY_NCR_CTRL3);
		writel(0x70026A33, phybase + USB_PHY_NCR_CTRL4);
		writel(0x00000001, phybase + USB_PHY_NCR_CTRL5);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL6);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL7);
	} else if (phy_num == 2 && clk_select == 1) {
		enable_clk(2, 0, 0);
		enable_clk(2, 1, 1);
		printf("\033[31musb phy 2 internal clk\033[00m\n");
		//writel(0x40000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x41000005, phybase + USB_PHY_NCR_CTRL0);
		writel(0x69254000, phybase + USB_PHY_NCR_CTRL1);
		//writel(0x0BAC7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x0E2C7878, phybase + USB_PHY_NCR_CTRL2);
		writel(0x3E700800, phybase + USB_PHY_NCR_CTRL3);
		writel(0x70026A33, phybase + USB_PHY_NCR_CTRL4);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL5);
		writel(0x00000001, phybase + USB_PHY_NCR_CTRL6);
		writel(0x00000000, phybase + USB_PHY_NCR_CTRL7);
	}

	if (1) {
		printf("\033[31musb init ctrl ncr\033[00m\n");
		writel(0x00000003, base + USB_CTRL_NCR_INTE);
		writel(0x00210080, base + USB_CTRL_NCR_CTRL0);
		writel(0x00000000, base + USB_CTRL_NCR_CTRL1);
		writel(0x80000000, base + USB_CTRL_NCR_CTRL2);
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
	usleep(30);

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

int main(int argc, char *argv[])
{
	void *base;
	int fd, i;
	unsigned int addr, usb_num, usb_mode, usb_speed, clk_select,  test_pattern, super_flag = 0;
	unsigned int *p;

	if (argc < 4) {
		printf("usb usb_num, usb_mode, usb_speed, clk_select, test_pattern, super_flag\n");
		printf("usb_mode 1: Device, 2: host\n");
		printf("usb_speed 1: full 2: high, 3: super, 0: low\n");
		printf("clk_select 1: internal 2: external\n");
		return 0;
	}
	if (argc > 1)
		usb_num = atoi(argv[1]);
	if (argc > 2)
		usb_mode = atoi(argv[2]);
	if (argc > 3)
		usb_speed = atoi(argv[3]);
	if (argc > 4)
		clk_select = atoi(argv[4]);
	if (argc > 5)
		test_pattern = atoi(argv[5]);
	if (argc > 6)
		super_flag = atoi(argv[6]);

	if (usb_num == 1)
		addr = 0x62320000;
	else
		addr = 0x62360000;
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

	usb_init(usb_num, base, usb_mode, usb_speed,  clk_select);
	printf("usb init ok\n");
	getchar();
	printf("put ENTER key enter to cp0 mode.......\n");
	getchar();
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
