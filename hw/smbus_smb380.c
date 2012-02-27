/*
 * SMB380 Sensor Emulation
 *
 * Contributed by Junsik.Park <okdear.park@samsung.com>
 */

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#else
#include <windows.h>
#include <winsock2.h>
#include <sys/types.h>
#endif

#include "hw.h"
#include "i2c.h"
#include "smbus.h"

//#define DEBUG

typedef struct SMBusSENSORDevice {
	SMBusDevice smbusedev;
	uint8_t data[7];

	int16_t x;
	int16_t y;
	int16_t z;
	int idx_out, req_out;
}SMBusSENSORDevice;

SMBusSENSORDevice glob_accel_state;

static int sensor_xyz_set(struct SMBusSENSORDevice *s, uint16_t x, uint16_t y, uint16_t z);

#define BITS_PER_BYTE 8
#define SMB380_ACCEL_BITS 10

static void smb380_quick_cmd(SMBusDevice *dev, uint8_t read)
{
#ifdef DEBUG
	printf("sensor quick cmd : addr=0x%02x read%d\n", dev->i2c.address, read);
#endif
}

static void smb380_send_byte(SMBusDevice *dev, uint8_t val)
{
#ifdef DEBUG
    SMBusSENSORDevice *s = (SMBusSENSORDevice *) dev;
    printf("smb380_send_byte: addr=0x%02x val=0x%02x index=%d\n",dev->i2c.address, val, s->idx_out);
#endif
    	
}

static uint8_t smb380_receive_byte(SMBusDevice *dev, uint8_t address)
{
    SMBusSENSORDevice *s = (SMBusSENSORDevice *) dev;
    int index = (int) address;
    if (index != 0)	
	    return s->data[index-2];
    else
	    return sensor_xyz_set(s, glob_accel_state.x, glob_accel_state.y, glob_accel_state.z);
}

void sensor_update(uint16_t x, uint16_t y, uint16_t z)
{
	glob_accel_state.x = x;
	glob_accel_state.y = y;
	glob_accel_state.z = z;
}

static int sensor_xyz_set(struct SMBusSENSORDevice *s, uint16_t x, uint16_t y, uint16_t z)
{
	/* remains right 10bit */

	x <<= (sizeof(uint16_t) * BITS_PER_BYTE - SMB380_ACCEL_BITS);
	x >>= (sizeof(uint16_t) * BITS_PER_BYTE - SMB380_ACCEL_BITS);

	y <<= (sizeof(uint16_t) * BITS_PER_BYTE - SMB380_ACCEL_BITS);
	y >>= (sizeof(uint16_t) * BITS_PER_BYTE - SMB380_ACCEL_BITS);

	z <<= (sizeof(uint16_t) * BITS_PER_BYTE - SMB380_ACCEL_BITS);
	z >>= (sizeof(uint16_t) * BITS_PER_BYTE - SMB380_ACCEL_BITS);

	/* data[1] : 2 ~ 10  (bit)
	 * data[0] : 0 ~ 1 (bit) */

	s->data[0] = (0x3 & x) << 6;
	s->data[1] = x >> 2;
	s->data[2] = (0x3 & y) << 6;
	s->data[3] = y >> 2;
	s->data[4] = (0x3 & z) << 6;
	s->data[5] = z >> 2;

	return 0;
}

static void smb380_reset(struct SMBusSENSORDevice *s)
{
	s->idx_out = 0;

	glob_accel_state.x = 0;
	glob_accel_state.y = -256;
	glob_accel_state.z = 0;

	s->data[0] = 0;
	s->data[1] = 0;
	s->data[2] = 0;
	s->data[3] = 0;
	s->data[4] = 0;
	s->data[5] = 0;
	s->idx_out = 0;

	return ;
}

static uint8_t smb380_read_data(SMBusDevice *opaque, uint8_t address, uint8_t offset)
{
    return smb380_receive_byte(opaque, address);
}

#ifdef DEBUG
static int print_hex(char *data, int len)
{
	return 0;
}
#endif

static int parse_val(char *buff, unsigned char find_data, char *parsebuff)
{
	int vvvi = 0;
	while (1) {
		if (vvvi > 40) {
			return -1;
		}
		if (buff[vvvi] == find_data) {
				
			vvvi++;
			strncpy(parsebuff, buff, vvvi);
			return vvvi;
		}
		vvvi++;
	}

	return 0;
}

#define  SENSOR_BUF				16
#define  SENSOR_VALUE_BUF		56
static int get_from_ide(struct SMBusSENSORDevice *accel_state) 
{
	int client_socket;
	struct 	sockaddr_in   server_addr;
	char	buff[SENSOR_VALUE_BUF + 1];
	char	buff2[SENSOR_VALUE_BUF + 1];
	char	tmpbuf[SENSOR_VALUE_BUF + 1];
	int		len = 0, len1 = 0;
	int		result = 0;
	const char *command0 = "readSensor()\n";
	const char *command1 = "accelerometer\n";

#ifdef _WIN32
	WSADATA wsaData;
	if(WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR)
		return -1;

	client_socket = socket(AF_INET, SOCK_STREAM, 0);
#else
	client_socket = socket(PF_INET, SOCK_STREAM, 0);
#endif
	if (client_socket == -1) {
		return -1;
	}

	memset(&server_addr, 0, sizeof( server_addr));
	server_addr.sin_family     = AF_INET;
	server_addr.sin_port       = htons(8010);
	server_addr.sin_addr.s_addr= inet_addr("127.0.0.1");
	if( -1 == connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
		close( client_socket);
		return -1;
	}

	if(read (client_socket, buff, SENSOR_BUF) < 0) {
		close(client_socket);
		return -1;
	}

	if(1) {
		if(write(client_socket, command0, strlen(command0)) < 0) {
		close(client_socket);
		return -1;
		}
		if(write(client_socket, command1, strlen(command1)) < 0) {
		close(client_socket);
		return -1;
		}

		memset(buff, '\0', sizeof(buff));
		memset(buff2, '\0', sizeof(buff2));

		result = read(client_socket, buff2, SENSOR_VALUE_BUF);
		if (result <= (SENSOR_VALUE_BUF)/2) {
			close( client_socket);
			return -1;
		}
		memcpy(buff, buff2, result);
	}

#ifdef DEBUG
	print_hex(buff2, 90);
#endif

	/* start */
	memset(tmpbuf, '\0', sizeof(tmpbuf));
	len = parse_val(buff2, 0x33, tmpbuf);

	memset(tmpbuf, '\0', sizeof(tmpbuf));
	len += parse_val(buff2+len, 0x0a, tmpbuf);

	/* first data */
	memset(tmpbuf, '\0', sizeof(tmpbuf));
	len1 = parse_val(buff2+len, 0x0a, tmpbuf);
	len += len1;
#ifdef DEBUG
	print_hex(tmpbuf, len1);
#endif
	//accel.read_accelx = atof(tmpbuf);
	accel_state->x = (int)(atof(tmpbuf) * (-26.2)); // 26 ~= 256 / 9.8
	if (accel_state->x > 512)
		accel_state->x = 512;
	if (accel_state->x < -512)
		accel_state->x = -512;

	/* second data */
	memset(tmpbuf, '\0', sizeof(tmpbuf));
	len1 = parse_val(buff2+len, 0x0a, tmpbuf);
	len += len1;
#ifdef DEBUG
	print_hex(tmpbuf, len1);
#endif
	accel_state->y = (int)(atof(tmpbuf) * 26.2);
	if (accel_state->y > 512)
		accel_state->y = 512;
	if (accel_state->y < -512)
		accel_state->y = -512;

	/* third data */
	memset(tmpbuf, '\0', sizeof(tmpbuf));
	len1 = parse_val(buff2+len, 0x0a, tmpbuf);
	len += len1;
#ifdef DEBUG
	print_hex(tmpbuf, len1);
#endif
	accel_state->z = (int)(atof(tmpbuf) * 26);

	if (accel_state->z > 512)
		accel_state->z = 512;
	if (accel_state->z < -512)
		accel_state->z = -512;

#ifdef DEBUG
	printf("accel_state->x=%d %d %d\n", accel_state->x, accel_state->y, accel_state->z);
#endif

	close( client_socket);

	return 0;
}

static void smb380_write_data(SMBusDevice *dev, uint8_t address, uint8_t offset, uint8_t val)
{
    SMBusSENSORDevice *s = (SMBusSENSORDevice *)dev;

#ifdef DEBUG
	printf("smb380_write\n");
#endif
	get_from_ide (&glob_accel_state);
		
	sensor_xyz_set (s, glob_accel_state.x, glob_accel_state.y, glob_accel_state.z);
		

#ifdef DEBUG
	printf("smb380_send_byte: addr=0x%02x val=0x%02x\n",address, val);
#endif
	smb380_send_byte(dev,val);
	
}

static int smb380_init(SMBusDevice *s)
{
    SMBusSENSORDevice *t = (SMBusSENSORDevice *)s;

    smb380_reset(t);
    return 0;
}

#if 0
#define DEFAULT_TIME 500000000
/* get sensor info from IDE when performance problem occurs */

static void sensor_timer (void *opaque)
{
    SensorState *s = opaque;
    int64_t expire_time = DEFAULT_TIME;

#ifdef DEBUG
    //printf("sensor timer\n");
#endif

    /* 1 sec : 500000000 */

    qemu_mod_timer (s->ts, qemu_get_clock (vm_clock) + expire_time);
}


/* set timer for getting sensor info */

SensorState* sensor_init()
{
    SensorState *s = &glob_accel_state;

    s->ts = qemu_new_timer (vm_clock, sensor_timer, s);
    qemu_mod_timer (s->ts, qemu_get_clock (vm_clock) + DEFAULT_TIME);

    return s;

    //s->ts = qemu_new_timer (vm_clock, accel_timer, s);
}
#endif


static SMBusDeviceInfo smb380_info = {
    .i2c.qdev.name = "smb380",
    .i2c.qdev.size = sizeof(SMBusSENSORDevice),
    .init = smb380_init,
    .quick_cmd = smb380_quick_cmd,
    .send_byte = smb380_send_byte,
    .receive_byte = smb380_receive_byte,
    .read_data = smb380_read_data,
    .write_data = smb380_write_data
};

static void smb380_register_devices(void)
{
    smbus_register_device(&smb380_info);
}

device_init(smb380_register_devices)
