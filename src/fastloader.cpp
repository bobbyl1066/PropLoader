#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "loader.h"

#define FINAL_BAUD              115200 //921600      /* Final XBee-to-Propeller baud rate. */
#define MAX_RX_SENSE_ERROR      23          /* Maximum number of cycles by which the detection of a start bit could be off (as affected by the Loader code) */

// Offset (in bytes) from end of Loader Image pointing to where most host-initialized values exist.
// Host-Initialized values are: Initial Bit Time, Final Bit Time, 1.5x Bit Time, Failsafe timeout,
// End of Packet timeout, and ExpectedID.  In addition, the image checksum at word 5 needs to be
// updated.  All these values need to be updated before the download stream is generated.
// NOTE: DAT block data is always placed before the first Spin method
#define RAW_LOADER_INIT_OFFSET_FROM_END (-(10 * 4) - 8)

// Raw loader image.  This is a memory image of a Propeller Application written in PASM that fits into our initial
// download packet.  Once started, it assists with the remainder of the download (at a faster speed and with more
// relaxed interstitial timing conducive of Internet Protocol delivery. This memory image isn't used as-is; before
// download, it is first adjusted to contain special values assigned by this host (communication timing and
// synchronization values) and then is translated into an optimized Propeller Download Stream understandable by the
// Propeller ROM-based boot loader.
static uint8_t rawLoaderImage[] = {
    0x00,0xB4,0xC4,0x04,0x6F,0x2B,0x10,0x00,0x88,0x01,0x90,0x01,0x80,0x01,0x94,0x01,
    0x78,0x01,0x02,0x00,0x70,0x01,0x00,0x00,0x4D,0xE8,0xBF,0xA0,0x4D,0xEC,0xBF,0xA0,
    0x51,0xB8,0xBC,0xA1,0x01,0xB8,0xFC,0x28,0xF1,0xB9,0xBC,0x80,0xA0,0xB6,0xCC,0xA0,
    0x51,0xB8,0xBC,0xF8,0xF2,0x99,0x3C,0x61,0x05,0xB6,0xFC,0xE4,0x59,0x24,0xFC,0x54,
    0x62,0xB4,0xBC,0xA0,0x02,0xBC,0xFC,0xA0,0x51,0xB8,0xBC,0xA0,0xF1,0xB9,0xBC,0x80,
    0x04,0xBE,0xFC,0xA0,0x08,0xC0,0xFC,0xA0,0x51,0xB8,0xBC,0xF8,0x4D,0xE8,0xBF,0x64,
    0x01,0xB2,0xFC,0x21,0x51,0xB8,0xBC,0xF8,0x4D,0xE8,0xBF,0x70,0x12,0xC0,0xFC,0xE4,
    0x51,0xB8,0xBC,0xF8,0x4D,0xE8,0xBF,0x68,0x0F,0xBE,0xFC,0xE4,0x48,0x24,0xBC,0x80,
    0x0E,0xBC,0xFC,0xE4,0x52,0xA2,0xBC,0xA0,0x54,0x44,0xFC,0x50,0x61,0xB4,0xFC,0xA0,
    0x5A,0x5E,0xBC,0x54,0x5A,0x60,0xBC,0x54,0x5A,0x62,0xBC,0x54,0x04,0xBE,0xFC,0xA0,
    0x54,0xB6,0xBC,0xA0,0x53,0xB8,0xBC,0xA1,0x00,0xBA,0xFC,0xA0,0x80,0xBA,0xFC,0x72,
    0xF2,0x99,0x3C,0x61,0x25,0xB6,0xF8,0xE4,0x36,0x00,0x78,0x5C,0xF1,0xB9,0xBC,0x80,
    0x51,0xB8,0xBC,0xF8,0xF2,0x99,0x3C,0x61,0x00,0xBB,0xFC,0x70,0x01,0xBA,0xFC,0x29,
    0x2A,0x00,0x4C,0x5C,0xFF,0xC2,0xFC,0x64,0x5D,0xC2,0xBC,0x68,0x08,0xC2,0xFC,0x20,
    0x55,0x44,0xFC,0x50,0x22,0xBE,0xFC,0xE4,0x01,0xB4,0xFC,0x80,0x1E,0x00,0x7C,0x5C,
    0x22,0xB6,0xBC,0xA0,0xFF,0xB7,0xFC,0x60,0x54,0xB6,0x7C,0x86,0x00,0x8E,0x68,0x0C,
    0x59,0xC2,0x3C,0xC2,0x09,0x00,0x54,0x5C,0x01,0xB2,0xFC,0xC1,0x63,0x00,0x70,0x5C,
    0x63,0xB4,0xFC,0x84,0x45,0xC6,0x3C,0x08,0x04,0x8A,0xFC,0x80,0x48,0x7E,0xBC,0x80,
    0x3F,0xB4,0xFC,0xE4,0x63,0x7E,0xFC,0x54,0x09,0x00,0x7C,0x5C,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x80,0x00,0x00,
    0xFF,0xFF,0xF9,0xFF,0x10,0xC0,0x07,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x40,
    0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x10,0x6F,0x00,0x00,0x00,0xB6,0x02,0x00,0x00,
    0x56,0x00,0x00,0x00,0x82,0x00,0x00,0x00,0x55,0x73,0xCB,0x00,0x18,0x51,0x00,0x00,
    0x30,0x00,0x00,0x00,0x30,0x00,0x00,0x00,0x68,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x35,0xC7,0x08,0x35,0x2C,0x32,0x00,0x00};
    
// Loader VerifyRAM snippet.
static uint8_t verifyRAM[] = {
    0x49,0xBC,0xBC,0xA0,0x45,0xBC,0xBC,0x84,0x02,0xBC,0xFC,0x2A,0x45,0x8C,0x14,0x08,
    0x04,0x8A,0xD4,0x80,0x66,0xBC,0xD4,0xE4,0x0A,0xBC,0xFC,0x04,0x04,0xBC,0xFC,0x84,
    0x5E,0x94,0x3C,0x08,0x04,0xBC,0xFC,0x84,0x5E,0x94,0x3C,0x08,0x01,0x8A,0xFC,0x84,
    0x45,0xBE,0xBC,0x00,0x5F,0x8C,0xBC,0x80,0x6E,0x8A,0x7C,0xE8,0x46,0xB2,0xBC,0xA4,
    0x09,0x00,0x7C,0x5C};
    
// Loader ProgramVerifyEEPROM snippet.
static uint8_t programVerifyEEPROM[] = {
    0x03,0x8C,0xFC,0x2C,0x4F,0xEC,0xBF,0x68,0x82,0x18,0xFD,0x5C,0x40,0xBE,0xFC,0xA0,
    0x45,0xBA,0xBC,0x00,0xA0,0x62,0xFD,0x5C,0x79,0x00,0x70,0x5C,0x01,0x8A,0xFC,0x80,
    0x67,0xBE,0xFC,0xE4,0x8F,0x3E,0xFD,0x5C,0x49,0x8A,0x3C,0x86,0x65,0x00,0x54,0x5C,
    0x00,0x8A,0xFC,0xA0,0x49,0xBE,0xBC,0xA0,0x7D,0x02,0xFD,0x5C,0xA3,0x62,0xFD,0x5C,
    0x45,0xC0,0xBC,0x00,0x5D,0xC0,0x3C,0x86,0x79,0x00,0x54,0x5C,0x01,0x8A,0xFC,0x80,
    0x72,0xBE,0xFC,0xE4,0x01,0x8C,0xFC,0x28,0x8F,0x3E,0xFD,0x5C,0x01,0x8C,0xFC,0x28,
    0x46,0xB2,0xBC,0xA4,0x09,0x00,0x7C,0x5C,0x82,0x18,0xFD,0x5C,0xA1,0xBA,0xFC,0xA0,
    0x8D,0x62,0xFD,0x5C,0x79,0x00,0x70,0x5C,0x00,0x00,0x7C,0x5C,0xFF,0xBD,0xFC,0xA0,
    0xA0,0xBA,0xFC,0xA0,0x8D,0x62,0xFD,0x5C,0x83,0xBC,0xF0,0xE4,0x45,0xBA,0x8C,0xA0,
    0x08,0xBA,0xCC,0x28,0xA0,0x62,0xCD,0x5C,0x45,0xBA,0x8C,0xA0,0xA0,0x62,0xCD,0x5C,
    0x79,0x00,0x70,0x5C,0x00,0x00,0x7C,0x5C,0x47,0x8E,0x3C,0x62,0x90,0x00,0x7C,0x5C,
    0x47,0x8E,0x3C,0x66,0x09,0xC0,0xFC,0xA0,0x58,0xB8,0xBC,0xA0,0xF1,0xB9,0xBC,0x80,
    0x4F,0xE8,0xBF,0x64,0x4E,0xEC,0xBF,0x78,0x56,0xB8,0xBC,0xF8,0x4F,0xE8,0xBF,0x68,
    0xF2,0x9D,0x3C,0x61,0x56,0xB8,0xBC,0xF8,0x4E,0xEC,0xBB,0x7C,0x00,0xB8,0xF8,0xF8,
    0xF2,0x9D,0x28,0x61,0x91,0xC0,0xCC,0xE4,0x79,0x00,0x44,0x5C,0x7B,0x00,0x48,0x5C,
    0x00,0x00,0x68,0x5C,0x01,0xBA,0xFC,0x2C,0x01,0xBA,0xFC,0x68,0xA4,0x00,0x7C,0x5C,
    0xFE,0xBB,0xFC,0xA0,0x09,0xC0,0xFC,0xA0,0x58,0xB8,0xBC,0xA0,0xF1,0xB9,0xBC,0x80,
    0x4F,0xE8,0xBF,0x64,0x00,0xBB,0x7C,0x62,0x01,0xBA,0xFC,0x34,0x4E,0xEC,0xBF,0x78,
    0x57,0xB8,0xBC,0xF8,0x4F,0xE8,0xBF,0x68,0xF2,0x9D,0x3C,0x61,0x58,0xB8,0xBC,0xF8,
    0xA7,0xC0,0xFC,0xE4,0xFF,0xBA,0xFC,0x60,0x00,0x00,0x7C,0x5C};

// Loader readyToLaunch snippet.
static uint8_t readyToLaunch[] = {
    0xB8,0x72,0xFC,0x58,0x66,0x72,0xFC,0x50,0x09,0x00,0x7C,0x5C,0x06,0xBE,0xFC,0x04,
    0x10,0xBE,0x7C,0x86,0x00,0x8E,0x54,0x0C,0x04,0xBE,0xFC,0x00,0x78,0xBE,0xFC,0x60,
    0x50,0xBE,0xBC,0x68,0x00,0xBE,0x7C,0x0C,0x40,0xAE,0xFC,0x2C,0x6E,0xAE,0xFC,0xE4,
    0x04,0xBE,0xFC,0x00,0x00,0xBE,0x7C,0x0C,0x02,0x96,0x7C,0x0C};

// Loader LaunchNow snippet.
static uint8_t launchNow[] = {
    0x66,0x00,0x7C,0x5C};

double ClockSpeed = 80000000.0;
int FinalBaud = FINAL_BAUD;

static uint8_t initCallFrame[] = {0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF, 0xF9, 0xFF};

static void SetHostInitializedValue(uint8_t *bytes, int offset, int value)
{
    for (int i = 0; i < 4; ++i)
        bytes[offset + i] = (value >> (i * 8)) & 0xFF;
}

static int32_t getLong(const uint8_t *buf)
{
     return (buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0];
}

static void setLong(uint8_t *buf, uint32_t value)
{
     buf[3] = value >> 24;
     buf[2] = value >> 16;
     buf[1] = value >>  8;
     buf[0] = value;
}

uint8_t *Loader::generateInitialLoaderImage(int packetID, int *pLength)
{
    int initAreaOffset = sizeof(rawLoaderImage) + RAW_LOADER_INIT_OFFSET_FROM_END;
    uint8_t *loaderImage;
    int checksum, i;

    // Allocate space for the image
    if (!(loaderImage = (uint8_t *)malloc(sizeof(rawLoaderImage))))
        return NULL;

    // Make a copy of the loader template
    memcpy(loaderImage, rawLoaderImage, sizeof(rawLoaderImage));
    
    // Clock mode
    //SetHostInitializedValue(loaderImage, initAreaOffset +  0, 0);

    // Initial Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  4, (int)trunc(80000000.0 / m_baudrate + 0.5));

    // Final Bit Time.
    SetHostInitializedValue(loaderImage, initAreaOffset +  8, (int)trunc(80000000.0 / FinalBaud + 0.5));
    
    // 1.5x Final Bit Time minus maximum start bit sense error.
    SetHostInitializedValue(loaderImage, initAreaOffset + 12, (int)trunc(1.5 * ClockSpeed / FinalBaud - MAX_RX_SENSE_ERROR + 0.5));
    
    // Failsafe Timeout (seconds-worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 16, (int)trunc(2.0 * ClockSpeed / (3 * 4) + 0.5));
    
    // EndOfPacket Timeout (2 bytes worth of Loader's Receive loop iterations).
    SetHostInitializedValue(loaderImage, initAreaOffset + 20, (int)trunc((2.0 * ClockSpeed / FinalBaud) * (10.0 / 12.0) + 0.5));
    
    // PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 24, Max(Round(ClockSpeed * SSSHTime), 14));
    // PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 28, Max(Round(ClockSpeed * SCLHighTime), 14));
    // PatchLoaderLongValue(RawSize*4+RawLoaderInitOffset + 32, Max(Round(ClockSpeed * SCLLowTime), 26));

    // Minimum EEPROM Start/Stop Condition setup/hold time (400 KHz = 1/0.6 µS); Minimum 14 cycles
    //SetHostInitializedValue(loaderImage, initAreaOffset + 24, 14);

    // Minimum EEPROM SCL high time (400 KHz = 1/0.6 µS); Minimum 14 cycles
    //SetHostInitializedValue(loaderImage, initAreaOffset + 28, 14);

    // Minimum EEPROM SCL low time (400 KHz = 1/1.3 µS); Minimum 26 cycles
    //SetHostInitializedValue(loaderImage, initAreaOffset + 32, 26);

    // First Expected Packet ID; total packet count.
    SetHostInitializedValue(loaderImage, initAreaOffset + 36, packetID);

    // Recalculate and update checksum so low byte of checksum calculates to 0.
    checksum = 0;
    loaderImage[5] = 0; // start with a zero checksum
    for (i = 0; i < (int)sizeof(rawLoaderImage); ++i)
        checksum += loaderImage[i];
    for (i = 0; i < (int)sizeof(initCallFrame); ++i)
        checksum += initCallFrame[i];
    loaderImage[5] = 256 - (checksum & 0xFF);
    
    /* return the loader image */
    *pLength = sizeof(rawLoaderImage);
    return loaderImage;
}

int Loader::fastLoadFile(const char *file, LoadType loadType)
{
    uint8_t *image;
    int imageSize;
    int sts;
    
    /* make sure the image was loaded into memory */
    if (!(image = readFile(file, &imageSize))) {
        printf("error: failed to load image '%s'\n", file);
        return -1;
    }
    
    /* load the file */
    sts = fastLoadImage(image, imageSize, loadType);
    free(image);
    
    /* return load result */
    return sts;
}

int Loader::fastLoadImage(const uint8_t *image, int imageSize, LoadType loadType)
{
    uint8_t *loaderImage, response[8];
    int loaderImageSize, result, cnt, i;
    int32_t packetID, checksum;
    
    /* compute the packet ID (number of packets to be sent) */
    packetID = (imageSize + m_connection->maxDataSize() - 1) / m_connection->maxDataSize();

    /* generate a loader image */
    loaderImage = generateInitialLoaderImage(packetID, &loaderImageSize);
    if (!loaderImage)
        return -1;
        
    /* compute the image checksum */
    checksum = 0;
    for (i = 0; i < imageSize; ++i)
        checksum += image[i];

    /* load the second-stage loader using the propeller ROM protocol */
    printf("Loading second-stage loader\n");
    result = m_connection->loadImage(loaderImage, loaderImageSize, ltDownloadAndRun);
    free(loaderImage);
    if (result != 0)
        return -1;
            
    printf("Waiting for second-stage loader initial response\n");
    cnt = m_connection->receiveDataExactTimeout(response, sizeof(response), 2000);
    result = getLong(&response[0]);
    if (cnt != 8 || result != packetID) {
        printf("error: second-stage loader failed to start - cnt %d, packetID %d, result %d\n", cnt, packetID, result);
        return -1;
    }
    printf("Got initial second-stage loader response\n");
    
    /* switch to the final baud rate */
    m_connection->setBaudRate(FINAL_BAUD);
    
    /* transmit the image */
    while (imageSize > 0) {
        int size;
        if ((size = imageSize) > m_connection->maxDataSize())
            size = m_connection->maxDataSize();
        if (transmitPacket(packetID, image, size, &result) != 0) {
            printf("error: transmitPacket failed\n");
            return -1;
        }
        if (result != packetID - 1)
            printf("Unexpected result: expected %d, received %d\n", packetID - 1, result);
        imageSize -= size;
        image += size;
        --packetID;
    }
    
    /*
        When we're doing a download that does not include an EEPROM write, the Packet IDs end up as:

        ltVerifyRAM: zero
        ltReadyToLaunch: -Checksum
        ltLaunchNow: -Checksum - 1

        ... and when we're doing a download that includes an EEPROM write, the Packet IDs end up as:

        ltVerifyRAM: zero
        ltProgramEEPROM: -Checksum
        ltReadyToLaunch: -Checksum*2
        ltLaunchNow: -Checksum*2 - 1
    */
    
    /* transmit the RAM verify packet and verify the checksum */
    printf("Verifying RAM\n");
    transmitPacket(packetID, verifyRAM, sizeof(verifyRAM), &result);
    if (result != -checksum)
        printf("Checksum error: expected %08x, got %08x\n", -checksum, result);
    packetID = -checksum;
    
    if (loadType & ltDownloadAndProgram) {
        printf("Programming EEPROM\n");
        transmitPacket(packetID, programVerifyEEPROM, sizeof(programVerifyEEPROM), &result, 8000);
        if (result != -checksum*2)
            printf("EEPROM programming error: expected %08x, got %08x\n", -checksum*2, result);
        packetID = -checksum*2;
    }
    
    /* transmit the final launch packets */
    
    printf("Sending readyToLaunch packet\n");
    transmitPacket(packetID, readyToLaunch, sizeof(readyToLaunch), &result);
    if (result != packetID - 1)
        printf("ReadyToLaunch failed: expected %08x, got %08x\n", packetID - 1, result);
    --packetID;
    
    printf("Sending launchNow packet\n");
    transmitPacket(packetID, launchNow, sizeof(launchNow), NULL);
    
    /* return successfully */
    return 0;
}

int Loader::transmitPacket(int id, const uint8_t *payload, int payloadSize, int *pResult, int timeout)
{
    int packetSize = 2*sizeof(uint32_t) + payloadSize;
    uint8_t *packet, response[8];
    int retries, result, cnt;
    int32_t tag;
    
    /* build the packet to transmit */
    if (!(packet = (uint8_t *)malloc(packetSize)))
        return -1;
    setLong(&packet[0], id);
    memcpy(&packet[8], payload, payloadSize);
    
    /* send the packet */
    retries = 3;
    while (--retries >= 0) {
    
        /* setup the packet header */
        tag = (int32_t)rand();
        setLong(&packet[4], tag);
        //printf("transmit packet %d\n", id);
        m_connection->sendData(packet, packetSize);
    
        /* receive the response */
        if (pResult) {
            cnt = m_connection->receiveDataExactTimeout(response, sizeof(response), timeout);
            result = getLong(&response[0]);
            if (cnt == 8 && getLong(&response[4]) == tag && result != id) {
                free(packet);
                *pResult = result;
                return 0;
            }
        }
        
        /* don't wait for a result */
        else {
            free(packet);
            return 0;
        }
    }
    
    /* free the packet */
    free(packet);
    
    /* return timeout */
    printf("error: transmitPacket %d failed\n", id);
    return -1;
}

