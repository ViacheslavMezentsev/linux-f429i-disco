#include "libtcc.c"
#include "runtime.h"
/* Lab driver: a relocation-free Thumb main returning an integer. */
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    TCCState *s = tcc_new();
    s->output_type = TCC_OUTPUT_OBJ;
    if (tcc_open(s, argv[1]) < 0 || tcc_compile(s)) return 3;
    tcc_close();
    int idx = find_elf_sym(symtab_section, "main");
    if (!idx || text_section->reloc || data_section->reloc || bss_section->data_offset) return 4;
    Elf32_Sym *sym = (Elf32_Sym *)symtab_section->data + idx;
    if (sym->st_shndx != text_section->sh_num || sym->st_value != 1) return 5;
    unsigned char header[64]={ 'b','F','L','T' };
    unsigned code = text_section->data_offset;
    unsigned runtime = (code + 3) & ~3u;
    unsigned runtime_size = puts_count ? sizeof puts_thumb : 0;
    unsigned data = runtime + runtime_size;
    for (unsigned i=0; i<puts_count; i++) {
        unsigned adr = puts_sites[i].adr, call = puts_sites[i].call;
        unsigned delta = data + puts_sites[i].data - ((adr+4) & ~3u);
        unsigned branch = runtime - (call+4);
        if (delta > 4095 || branch > 4094 || (branch & 1)) return 7;
        unsigned short *a = (unsigned short *)(text_section->data+adr);
        a[0] = 0xf20f | ((delta >> 11) << 10);
        a[1] = ((delta & 0x700) << 4) | (delta & 255);
        unsigned short *b = (unsigned short *)(text_section->data+call);
        b[1] = 0xf800 | (branch >> 1);
    }
    unsigned size = 72 + data + data_section->data_offset;
    unsigned fields[] = {4,65,size,size,size,4096,size,0,1};
    for (unsigned i=0;i<9;i++)
        for (unsigned j=0;j<4;j++) header[4+i*4+j]=fields[i]>>(24-8*j);
    unsigned char entry[]={0x00,0xf0,0x02,0xf8,0x01,0x27,0x00,0xdf};
    int fd=open(argv[2],O_CREAT|O_EXCL|O_WRONLY,0755);
    if(fd<0 || write(fd,header,64)!=64 || write(fd,entry,8)!=8 ||
       write(fd,text_section->data,text_section->data_offset)!=text_section->data_offset) return 6;
    unsigned char pad[3] = {0};
    if (write(fd,pad,runtime-code) != runtime-code ||
        write(fd,puts_thumb,runtime_size) != runtime_size ||
        write(fd,data_section->data,data_section->data_offset) != data_section->data_offset) return 6;
    return close(fd)!=0;
}
