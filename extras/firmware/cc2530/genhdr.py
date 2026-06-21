data = open("cc2530_radio.bin","rb").read()
with open("../../../examples/CC2530_FlashFirmware/cc2530_radio_fw.h","w") as o:
    o.write("const uint8_t FW[] = {\n")
    for i in range(0,len(data),12):
        o.write("  " + ", ".join("0x%02x" % b for b in data[i:i+12]) + ",\n")
    o.write("};\n")
    o.write("const unsigned int FW_LEN = %d;\n" % len(data))
print("regenerated cc2530_radio_fw.h: FW_LEN=%d" % len(data))
