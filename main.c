#include <stdio.h>
#include <sys/io.h>

#include <errno.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <values.h>

#include "pci.h"
#include "constants.h"


void getDeviceInfo();
ui getRegInfo(us);
bool isBridge();
uc getHeaderType();

void writeGeneralInfo(ui);
void writeClassCodeInfo(ui);
void writeBISTInfo(ui);
void writeSubSystemInfo(ui, us, us);
void writeCacheLineSizeInfo(ui);
void writeFullBusNumberInfo(ui);
void writeBusNumberInfo(char *, uc ,ui);
void writeIOLimitBaseInfo(ui);
void writeMemoryInfo(ui);
void writeCapabilitiesInfo(ui);
void writePrefetchableMemoryInfo(ui);
void writeBARsInfo(bool);
void writeIOMemorySpaceBARInfo(ui);
void writeMemorySpaceBARInfo(ui);
void writeISInfo(ui);
void writeExpansionROMBaseAddress(ui);
void writeGNTLATInfo(ui);
void writeInterruptPinInfo(ui);
void writeInterruptLineInfo(ui);

char *getVendorName(us);
char *getDeviceName(us, us);
char *getSubDeviceName(us, us, us ,us);
char *getBaseClassData(uc);
char *getSubclassData(uc, uc);
char *getSRLPIData(uc ,uc, uc);

FILE *out;
us bus, device, func;


int main(int argc, char *argv[])
{
    if (iopl(3)) {
        printf("I/O Privilege level change error: %s\nTry running under ROOT user\n", strerror(errno));
        return 2;
    }

    out = fopen("out_c.txt", "w");

    puts("Executing...");

    for (bus = 0; bus < MAX_BUS; bus++)
        for (device = 0; device < MAX_DEVICE; device++)
            for (func = 0; func < MAX_FUNCTIONS; func++)
                getDeviceInfo();

    puts("Press any key to exit...");
    getchar();
    fclose(out);
    return 0;
}

void getDeviceInfo()
{
    ui regInfo = getRegInfo(ID_REGISTER);
    if (regInfo != NO_DEVICE) {
        writeGeneralInfo(regInfo);
        writeBISTInfo(getRegInfo(BIST_REGISTER));
        bool bridge = isBridge();
        fprintf(out, "%s", bridge ? "\nBridge\n\n" : "\nNot bridge\n\n");
        writeClassCodeInfo(getRegInfo(CLASS_CODE_REGISTER));
        writeCacheLineSizeInfo(getRegInfo(CACHE_LINE_SIZE_REGISTER));
        writeBARsInfo(bridge);
        writeExpansionROMBaseAddress(getRegInfo(bridge ? (us)(EXPANSION_ROM_BASE_ADDRESS_REGISTER + 2) : (us)EXPANSION_ROM_BASE_ADDRESS_REGISTER));
        writeCapabilitiesInfo(getRegInfo(CAPABILITIES_REGISTER));
        if (bridge) {
            writeFullBusNumberInfo(getRegInfo(BUS_NUMBER_REGISTER));
            writeIOLimitBaseInfo(getRegInfo(IO_DATA_REGISTER));
            writeMemoryInfo(getRegInfo(MEMORY_DATA_REGISTER));
            writePrefetchableMemoryInfo(getRegInfo(PREF_MEMORY_DATA_REGISTER));
        } else {
            writeISInfo(getRegInfo(IS_POINTER_REGISTER));
            writeGNTLATInfo(getRegInfo(GNT_LAT_REGISTER));
        }
        writeInterruptPinInfo(getRegInfo(INTERRUPT_PIN_REGISTER));
        writeInterruptLineInfo(getRegInfo(INTERRUPT_LINE_REGISTER));
        fputs("-------------------------\n", out);
    }
}

ui getRegInfo(us reg)
{
    outl((1 << 31) | (bus << BUS_SHIFT) | (device << DEVICE_SHIFT) |
         (func << FUNCTION_SHIFT) | (reg << REGISTER_SHIFT), COMMAND_PORT);
    return inl(DATA_PORT);
}

bool isBridge()
{
    fprintf(out, "Multiple functions: %s\n", (getHeaderType() >> MULTIPLE_FUNCTIONS_SHIFT) ? "yes" : "no");
    return getHeaderType() & 1;
}

uc getHeaderType()
{
    return (getRegInfo(HEADER_TYPE_REGISTER) >> HEADER_TYPE_SHIFT) & UCHAR_MAX;
}

void writeGeneralInfo(ui regData)
{
    fprintf(out, "%X:%X:%X\n", bus, device, func);
    us deviceID = regData >> DEVICEID_SHIFT;
    us vendorID = regData & USHRT_MAX;
    char *vendorName = getVendorName(vendorID);
    fprintf(out, "Vendor ID: %04X, %s\n", vendorID, vendorName ? vendorName : "Unknown Vendor");
    char *deviceName = getDeviceName(vendorID, deviceID);
    fprintf(out, "Device ID: %04X, %s\n", deviceID, deviceName ? deviceName : "Unknown Device");
    if(!isBridge())
        writeSubSystemInfo(getRegInfo(SUBSYSTEM_DATA_REGISTER), deviceID, vendorID);
}

void writeSubSystemInfo(ui regData, us deviceID, us vendorID){
    us subVenID = regData & USHRT_MAX;
    us subDevID = regData >> SUBSYSTEMID_SHIFT;
    fprintf(out, "Subsystem Vendor ID: %#x\n", subVenID);
    fprintf(out, "\tSubVendorName: %s\n", getVendorName(subVenID));
    fprintf(out, "Subsystem Device ID: %#x\n", subDevID);
    char *subDevName = getSubDeviceName(deviceID, vendorID, subVenID, subDevID);
    fprintf(out, "\tSubDeviceName: %s\n", subDevName ? subDevName : "Unknown");
}

void writeClassCodeInfo(ui regData)
{
    uc revID = regData & UCHAR_MAX;

    ui classCode = (regData >> CLASS_CODE_SHIFT);
    uc baseClass = (classCode >> BASE_CLASS_SHIFT) & UCHAR_MAX;
    uc subclass = (classCode >> SUBCLASS_SHIFT) & UCHAR_MAX;
    uc srlPI = classCode & UCHAR_MAX;

    fprintf(out, "Revision ID: %#x\n", revID);
    fprintf(out, "Class code: %#x\n", classCode);
    fprintf(out, "Base class: %#x %s\n", baseClass, getBaseClassData(baseClass));
    fprintf(out, "Subclass: %#x %s\n", subclass, getSubclassData(baseClass, subclass));
    fprintf(out, "SRL PI: %#x %s\n", srlPI, getSRLPIData(baseClass, subclass, srlPI));
}

void writeBISTInfo(ui regData){
    uc bistStatus = regData >> BIST_SHIFT;
    if ((bistStatus)){
        fprintf(out, "\nBIST: available\n");
    }else{
        fprintf(out, "\nBIST: unavailable\n");
    }
}

void writeCacheLineSizeInfo(ui regData)
{
    uc cacheLineSize = regData & UCHAR_MAX;
    fprintf(out, "CLS: %d bytes\n", cacheLineSize * 4);
}

void writeFullBusNumberInfo(ui regData)
{
    writeBusNumberInfo(PRIMARY_BUS_NUMBER, PRIMARY_BUS_NUMBER_SHIFT, regData);
    writeBusNumberInfo(SECONDARY_BUS_NUMBER, SECONDARY_BUS_NUMBER_SHIFT, regData);
    writeBusNumberInfo(SUBORDINATE_BUS_NUMBER, SUBORDINATE_BUS_NUMBER_SHIFT, regData);
}

void writeBusNumberInfo(char *text, uc shift, ui regData)
{
    uc busNumber = (regData >> shift) & UCHAR_MAX;
    fprintf(out, text, busNumber);
}

void writeIOLimitBaseInfo(ui regData)
{
    uc IOBase = regData & UCHAR_MAX;
    uc IOLimit = (regData >> IO_LIMIT_SHIFT) & UCHAR_MAX;
    fprintf(out, "I/O Base: %#x\n", IOBase);
    fprintf(out, "I/O Limit: %#x\n", IOLimit);
}

void writeMemoryInfo(ui regData)
{
    us memoryBase = regData & USHRT_MAX;
    us memoryLimit = (regData >> MEMORY_LIMIT_SHIFT) & USHRT_MAX;
    fprintf(out, "Memory base: %#x\n", memoryBase);
    fprintf(out, "Memory limit: %#x\n", memoryLimit);
}

void writePrefetchableMemoryInfo(ui regData)
{
    us memoryBase = regData & USHRT_MAX;
    us memoryLimit = (regData >> MEMORY_LIMIT_SHIFT) & USHRT_MAX;
    fprintf(out, "Prefetchable memory base: %#x\n", memoryBase);
    fprintf(out, "Prefetchable memory limit: %#x\n", memoryLimit);

    if ((memoryBase & 1) || (memoryLimit % 1)){
        fprintf(out, "\tPrefetchable Upper Base: %#x\n", getRegInfo(UPPER_BASE_REGISTER));
        fprintf(out, "\tPrefetchable Upper Limit: %#x\n", getRegInfo(UPPER_LIMIT_REGISTER));
        ui upperIOData = getRegInfo(UPPER_IO_REGISTER);
        us upperLimit = (upperIOData >> UPPER_LIMIT_SHIFT) & USHRT_MAX;
        us upperBase = upperIOData & USHRT_MAX;
        fprintf(out, "\t\tIO Upper base: %#x\n", upperBase);
        fprintf(out, "\t\tIO Upper limit: %#x\n", upperLimit);
    }
}

void writeBARsInfo(bool bridge)
{
    fputs("BARs:\n", out);
    int barQuantity = bridge ? BAR_BRIDGE_QUANTITY : BAR_NBRIDGE_QUANTITY;
    for (int i = 0; i < barQuantity; i++) {
        fprintf(out, "\tRegister #%d: ", i + 1);
        ui regData = getRegInfo(FIRST_BAR_REGISTER + i);
        if (regData) {
            if (regData & 1) {
                writeIOMemorySpaceBARInfo(regData);
            } else {
                writeMemorySpaceBARInfo(regData);
            }
        } else {
            fputs("unused register\n", out);
        }
    }
}

void writeMemorySpaceBARInfo(ui regData)
{
    fputs("Memory space register\n", out);
    uc typeBits = (regData >> TYPE_BITS_SHIFT) & 3;
    fputs("\t\tAddress range of memory block: ", out);
    switch (typeBits) {
        case 0:
            fputs("any position in 32 bit address space\n", out);
            break;
        case 1:
            fputs("below 1MB\n", out);
            break;
        case 2:
            fputs("any position in 64 bit address space\n", out);
            break;
        default:
            fputs("-\n", out);
            break;
    }
    fprintf(out, "\t\tAddress: %#x\n", regData >> MEMORY_SPACE_BAR_ADDRESS_SHIFT);
    fprintf(out, "\t\tPrefetchable: %s\n", (regData & PREF_BAR) ? "yes" : "no");
}

void writeIOMemorySpaceBARInfo(ui regData)
{
    fputs("I/O space register\n", out);
    fprintf(out, "\t\tAddress: %#x\n", regData >> IO_MEMORY_SPACE_BAR_ADDRESS_SHIFT);
}


void writeISInfo(ui regData){
    fprintf(out, "Cardbus Card IS Pointer: ");
    switch(regData & 3){
        case 0:
            fprintf(out, "in device's device-specific configuration space\n");
            break;
        case 1:
            fprintf(out, "in memory pointed to by base address register 0\n");
            break;
        case 2:
            fprintf(out, "in memory pointed to by base address register 1\n");
            break;
        case 3:
            fprintf(out, "in memory pointed to by base address register 2\n");
            break;
        case 4:
            fprintf(out, "in memory pointed to by base address register 3\n");
            break;
        case 5:
            fprintf(out, "in memory pointed to by base address register 4\n");
            break;
        case 6:
            fprintf(out, "in memory pointed to by base address register 5\n");
            break;
        case 7:
            fprintf(out, "in device's expansion ROM\n");
            fprintf(out, "\t\tROM image number: %#X\n", regData >> 27);
            break;
    }
}

void writeExpansionROMBaseAddress(ui regData)
{
    if (regData & 1) {
        fprintf(out, "Expansion ROM BAR: %#x\n", regData >> EXPANSION_ROM_BASE_ADDRESS_SHIFT);
    } else {
        fputs("Expansion ROM BAR: address space is disabled or virtualized\n", out);
    }
}

void writeCapabilitiesInfo(ui regData){
    fprintf(out, "Capabilities list: ");
    if (getRegInfo(STATUS_REGISTER) >> 3) {
        uc capabData = regData & UCHAR_MAX;
        fprintf(out, "available, pointer value: %#x\n", capabData);
    }else{
        fprintf(out, "unavailable\n");
    }
}

void writeGNTLATInfo(ui regData){
    uc gntData = (regData >> GNT_SHIFT) & UCHAR_MAX;
    uc latData = (regData >> LAT_SHIFT) & UCHAR_MAX;
    fprintf(out, "Min GNT: %#X\n", gntData);
    fprintf(out, "Max latency: %#X\n", latData);
}

void writeInterruptPinInfo(ui regData)
{
    uc interruptPin = (regData >> INTERRUPT_PIN_SHIFT) & UCHAR_MAX;
    char *interruptPinData;
    switch (interruptPin) {
        case 0:
            interruptPinData = "-";
            break;
        case 1:
            interruptPinData = "INTA#";
            break;
        case 2:
            interruptPinData = "INTB#";
            break;
        case 3:
            interruptPinData = "INTC#";
            break;
        case 4:
            interruptPinData = "INTD#";
            break;
        default:
            interruptPinData = "invalid pin number";
            break;
    }
    fprintf(out, "Interrupt pin: %s\n", interruptPinData);
}

void writeInterruptLineInfo(ui regData)
{
    uc interruptLine = regData & UCHAR_MAX;
    fputs("Interrupt line: ", out);
    if (interruptLine == UCHAR_MAX) {
        fputs("unused/unknown\n", out);
    } else if (interruptLine < INTERRUPT_LINES_NUMBER) {
        if(interruptLine)
            fprintf(out, "%s%d\n", "IRQ", interruptLine);
        else
            fprintf(out, "IRQ0/APIC-Connected/no line\n");
    } else {
        fputs("invalid line number\n", out);
    }
}

char *getVendorName(us vID)
{
    for(int i = 0; i < PCI_VENTABLE_LEN; i++){
        if (vID == PciVenTable[i].VendorId) {
            return PciVenTable[i].VendorName;
        }
    }
    return NULL;
}

char *getDeviceName(us vID, us dID)
{
    for(int i = 0; i < PCI_DEVTABLE_LEN; i++){
        if (vID == PciDevTable[i].VendorId && dID == PciDevTable[i].DeviceId) {
            return PciDevTable[i].DeviceName;
        }
    }
    return NULL;
}

char *getSubDeviceName(us devID, us venID, us subVenID, us subDevID)
{
    for(int i = 0; i < PCI_SUBTABLE_LEN; i++){
        if (venID == PciSubTable[i].VendorId && devID == PciSubTable[i].DeviceId
            && subVenID == PciSubTable[i].SubVendorId && subDevID == PciSubTable[i].SubDeviceId) {
            return PciSubTable[i].SubDeviceName;
        }
    }
    return NULL;
}

char *getBaseClassData(uc bClass)
{
    for(int i = 0; i < PCI_CLASSCODETABLE_LEN; i++){
        if (bClass == PciClassCodeTable[i].BaseClass) {
            return PciClassCodeTable[i].BaseDesc;
        }
    }
    return NULL;
}

char *getSubclassData(uc bClass, uc sClass)
{
    for(int i = 0; i < PCI_CLASSCODETABLE_LEN; i++){
        if (bClass == PciClassCodeTable[i].BaseClass && sClass == PciClassCodeTable[i].SubClass) {
            return PciClassCodeTable[i].SubDesc;
        }
    }
    return NULL;
}

char *getSRLPIData(uc bClass, uc sClass, uc srlPI)
{
    for(int i = 0; i < PCI_CLASSCODETABLE_LEN; i++){
        if (bClass == PciClassCodeTable[i].BaseClass && sClass == PciClassCodeTable[i].SubClass
        && srlPI == PciClassCodeTable[i].ProgIf) {
            return PciClassCodeTable[i].ProgDesc;
        }
    }
    return NULL;
}