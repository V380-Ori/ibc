#include <IOKit/IOKitLib.h>
#include <mach/mach_error.h>
#include <stdio.h>
#include <stdlib.h>

#include "SMCParamStruct.h"

#define KEY_TO_CHARS(k) (char)((k)>>24), (char)((k)>>16), (char)((k)>>8), (char)(k)

/* SMC key for macOS 15.7+ */
#define CHTE           'CHTE'
#define CHTE_DATA_TYPE 'ui32'
#define CHTE_DATA_SIZE 4
#define CHTE_DATA_ATTR 0xD4

/* Legacy SMC key */
#define CH0C           'CH0C'
#define CH0C_DATA_TYPE 'hex_'
#define CH0C_DATA_SIZE 1
#define CH0C_DATA_ATTR 0xD4

static kern_return_t connect_to_smc(io_connect_t *connect);
static void          disconnect_from_smc(io_connect_t *connect);
static kern_return_t toggle_smc_key(io_connect_t connect, uint32_t key, uint32_t type, uint8_t size, uint8_t attr);

int main() {
    io_connect_t smc_conn = MACH_PORT_NULL;
    kern_return_t ret;

    ret = connect_to_smc(&smc_conn);

    if (ret != KERN_SUCCESS) {
        return EXIT_FAILURE;
    }

    ret = toggle_smc_key(smc_conn,
                         CH0C,
                         CH0C_DATA_TYPE,
                         CH0C_DATA_SIZE,
                         CH0C_DATA_ATTR);

    if (ret == kSMCKeyNotFound) {
        ret = toggle_smc_key(smc_conn,
                             CHTE,
                             CHTE_DATA_TYPE,
                             CHTE_DATA_SIZE,
                             CHTE_DATA_ATTR);
    }

    disconnect_from_smc(&smc_conn);

    return ret != KERN_SUCCESS;
}

static kern_return_t connect_to_smc(io_connect_t *connect) {
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSMC"));

    if (service == IO_OBJECT_NULL) {
        fprintf(stderr, "Error: AppleSMC service not found.\n");

        return KERN_FAILURE;
    }

    kern_return_t ret = IOServiceOpen(service, mach_task_self(), 1, connect);

    IOObjectRelease(service);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "Failed to open connection to AppleSMC service: %s\n", mach_error_string(ret));

        return ret;
    }

    ret = IOConnectCallMethod(*connect, kSMCUserClientOpen, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "Failed to open AppleSMC user client: %s\n", mach_error_string(ret));

        IOServiceClose(*connect);

        *connect = MACH_PORT_NULL;
    }

    return ret;
}

static void disconnect_from_smc(io_connect_t *connect) {
    if (*connect == MACH_PORT_NULL) {
        return;
    }

    IOConnectCallMethod(*connect, kSMCUserClientClose, NULL, 0, NULL, 0, NULL, NULL, NULL, NULL);

    IOServiceClose(*connect);

    *connect = MACH_PORT_NULL;
}

static kern_return_t toggle_smc_key(io_connect_t connect, uint32_t key, uint32_t type, uint8_t size, uint8_t attr) {
    SMCParamStruct input = {}, output = {};
    size_t output_size = sizeof(SMCParamStruct);

    input.key = key;
    input.data8 = kSMCGetKeyInfo;

    kern_return_t ret = IOConnectCallStructMethod(connect, kSMCHandleYPCEvent, &input, sizeof(input), &output, &output_size);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "Failed to get SMC key info: %s\n", mach_error_string(ret));

        return KERN_FAILURE;
    }

    if (output.result == kSMCKeyNotFound) {
        return kSMCKeyNotFound;
    }

    if (output.result != kSMCSuccess) {
        fprintf(stderr, "Error: SMC returned unsuccessful result on key info.\n");

        return KERN_FAILURE;
    }

    if (output.keyInfo.dataType != type || output.keyInfo.dataSize != size || output.keyInfo.dataAttributes != attr) {
        fprintf(stderr, "Error: SMC key info does not match expected values.\n");

        return KERN_FAILURE;
    }

    input.data8 = kSMCReadKey;
    input.keyInfo.dataSize = size;

    ret = IOConnectCallStructMethod(connect, kSMCHandleYPCEvent, &input, sizeof(input), &output, &output_size);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "Failed to read SMC key: %s\n", mach_error_string(ret));

        return KERN_FAILURE;
    }

    if (output.result != kSMCSuccess) {
        fprintf(stderr, "Error: SMC returned unsuccessful result on key read.\n");

        return KERN_FAILURE;
    }

    input.data8 = kSMCWriteKey;

    if (size == CH0C_DATA_SIZE) {
        input.bytes[0] = !output.bytes[0];
    } else if (size == CHTE_DATA_SIZE) {
        *(uint32_t*)input.bytes = !*(uint32_t*)output.bytes;
    }

    ret = IOConnectCallStructMethod(connect, kSMCHandleYPCEvent, &input, sizeof(input), &output, &output_size);

    if (ret != KERN_SUCCESS) {
        fprintf(stderr, "Failed to write to SMC key: %s\n", mach_error_string(ret));

        return KERN_FAILURE;
    }

    if (output.result != kSMCSuccess) {
        fprintf(stderr, "Error: SMC returned unsuccessful result on key write.\n");

        return KERN_FAILURE;
    }

    if (size == CH0C_DATA_SIZE) {
        fprintf(stdout, "%c%c%c%c -> %02x\n", KEY_TO_CHARS(key), input.bytes[0]);
    } else if (size == CHTE_DATA_SIZE) {
        fprintf(stdout, "%c%c%c%c -> %08x\n", KEY_TO_CHARS(key), *(uint32_t*)input.bytes);
    }

    return KERN_SUCCESS;
}
