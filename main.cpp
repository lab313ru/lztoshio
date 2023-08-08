//#define ADD_EXPORTS

#include "main.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define wndsize (1 << 12)
#define wndmask (wndsize - 1)

#define maxreps1 (0xF + 3)

byte read_byte(byte *input, int *readoff)
{
    return (input[(*readoff)++]);
}

void write_byte(byte *output, int *writeoff, byte b)
{
    output[(*writeoff)++] = b;
}

byte read_wnd_byte(byte *window, ushort *wndoff)
{
    byte b = window[*wndoff];
    *wndoff = (*wndoff + 1) & wndmask;
    return b;
}

void write_to_wnd(byte *window, ushort *wndoff, byte b)
{
    window[*wndoff] = b;
    *wndoff = (*wndoff + 1) & wndmask;
}

ushort read_word_le(byte *input, int *readoff)
{
    ushort retn = *(ushort*)(&input[*readoff]);
    *readoff += sizeof(ushort);
    return retn;
}

void write_word_le(byte *output, int *writeoff, ushort w)
{
    *(ushort*)(&output[*writeoff]) = w;
    *writeoff += sizeof(ushort);
}

uint read_dword_le(byte *input, int *readoff)
{
    uint retn = *(uint*)(&input[*readoff]);
    *readoff += sizeof(uint);
    return retn;
}

void write_dword_le(byte *output, int *writeoff, uint d)
{
    *(uint*)(&output[*writeoff]) = d;
    *writeoff += sizeof(uint);
}

int read_cmd_bit(byte *input, int *readoff, byte *bitscnt, byte *cmd)
{
    (*bitscnt)--;

    if (!*bitscnt)
    {
        *cmd = read_byte(input, readoff);
        *bitscnt = 8;
    }

    int retn = *cmd & 1;
    *cmd >>= 1;
    return retn;
}

void write_cmd_bit(int bit, byte *output, int *writeoff, byte *bitscnt, int *cmdoff)
{
    if (*bitscnt == 8)
    {
        *bitscnt = 0;
        *cmdoff = (*writeoff)++;
        output[*cmdoff] = 0;
    }

    output[*cmdoff] = ((bit & 1) << *bitscnt) | output[*cmdoff];
    bit >>= 1;
    (*bitscnt)++;
}

void read_token(byte *input, int *readoff, byte *reps, ushort *from)
{
    ushort t = read_word_le(input, readoff);

    // 1111rrrr 11111111
    *reps = ((t & 0x0F00) >> 8) + 3;
    // ffff     ffffffff
    *from = ((t & 0xF000) >> 4) | (t & 0xFF);
}

void write_token(byte *output, int *writeoff, byte reps, ushort from)
{
    ushort t = ((reps - 3) << 8) & 0x0F00;
    t |= ((from & 0x0F00) << 4) | (from & 0xFF);
    write_word_le(output, writeoff, t);
}

void init_wnd(byte **window, ushort *wndoff)
{
    *window = (byte *)malloc(wndsize);
    
    for (int i = 0; i < 0x100; ++i)
    {
        for (int j = 0; j < 0x0D; ++j)
        {
            (*window)[i * 0x0D + j] = i;
        }
    }

    for (int i = 0; i < 0x100; ++i)
    {
        (*window)[0xD00 + i] = i;
    }

    for (int i = 0; i < 0x100; ++i)
    {
        (*window)[0xE00 + i] = (0xFF - i);
    }

    for (int i = 0; i < 0x80; ++i)
    {
        (*window)[0xF00 + i] = 0x00;
    }

    for (int i = 0; i < 0x6E; ++i)
    {
        (*window)[0xF80 + i] = 0x20;
    }

    for (int i = 0; i < 0x12; ++i)
    {
        (*window)[0xFEE + i] = 0x00;
    }

    *wndoff = 0xFEE;
}

void find_matches(byte *input, int readoff, int size, int wndoff, byte *window, byte *reps, ushort *from, int min_pos, int max_pos)
{
    int wpos = 0, tlen = 0;

    *reps = 1;
    wpos = min_pos;

    while (wpos < max_pos && tlen < maxreps1)
    {
        tlen = 0;
        while (readoff + tlen < size && tlen < maxreps1)
        {
            if (((wpos + tlen) & wndmask) == wndoff && tlen != 0)
            {
                int index = 0;
                while ((readoff + tlen < size && tlen < maxreps1) &&
                    input[readoff + index] == input[readoff + tlen])
                {
                    tlen++;
                    index++;
                }
                break;
            }
            else if (window[(wpos + tlen) & wndmask] == input[readoff + tlen])
            {
                tlen++;
            }
            else
            {
                break;
            }
        }

        if (tlen >= *reps)
        {
            *reps = tlen;
            *from = wpos & wndmask;
        }

        wpos++;
    }
}

void reinit_and_find(byte *input, int size, int offset, byte *reps, ushort *from)
{
    int  i = 0, readoff = 0;
    byte b = 0;
    byte *window;
    ushort wndoff = 0;

    init_wnd(&window, &wndoff);

    for (i = 0; i < offset; ++i) {
        b = read_byte(input, &readoff);
        write_to_wnd(window, &wndoff, b);
    }

    find_matches(input, readoff, size, wndoff, window, reps, from, 0, wndsize);
}

int ADDCALL compress(byte *input, byte *output, int size)
{
    int i = 0, readoff = 0, writeoff = 0, cmdoff = 0;
    ushort wndoff = 0, from = 0;
    byte b = 0, bitscnt = 0, reps = 0;
    byte *window;

    init_wnd(&window, &wndoff);

    readoff = 0;
    writeoff = 9;

    cmdoff = 8;
    output[cmdoff] = 0;

    while (readoff < size)
    {
        if (readoff < 0x12)
        {
            find_matches(input, readoff, size, wndoff, window, &reps, &from, 0, wndsize - (0x12 - readoff));
        }
        else
        {
            find_matches(input, readoff, size, wndoff, window, &reps, &from, 0, wndsize);
        }

        if (reps <= 2)
        {
            write_cmd_bit(1, output, &writeoff, &bitscnt, &cmdoff);
            b = read_byte(input, &readoff);
            write_byte(output, &writeoff, b);
            write_to_wnd(window, &wndoff, b);

            // printf("off: %x; reps: %d\n", writeoff, 1);
            //printf("off: %d; reps: %d; from: %d\n", writeoff, 1, 0);
        }
        else
        {
            write_cmd_bit(0, output, &writeoff, &bitscnt, &cmdoff);
            write_token(output, &writeoff, reps, from);
            readoff += reps;

            for (i = 0; i < reps; ++i)
            {
                b = read_wnd_byte(window, &from);
                write_to_wnd(window, &wndoff, b);
            }

            //printf("off: %x; reps: %d\n", writeoff, reps);
            //printf("off: %d; reps: %d; from: %d\n", writeoff, reps, from);
        }
    }

    free(window);

    int retn = writeoff;
    writeoff = 0;
    write_dword_le(output, &writeoff, retn - 8); // enc_size
    write_dword_le(output, &writeoff, size); // dec_size
    if (retn & 1)
    {
        output[retn] = 0x00;
    }
    return (retn & 1) ? retn + 1 : retn;
}

int ADDCALL decompress(byte *input, byte *output)
{
    int enc_size = 0, dec_size = 0, readoff = 0, writeoff = 0;
    byte b = 0, bit = 0, bitscnt = 0, cmd = 0, reps = 0;
    ushort wndoff = 0, from = 0;
    byte *window;

    init_wnd(&window, &wndoff);

    readoff = 0;
    writeoff = 0;
    enc_size = read_dword_le(input, &readoff) + 8;
    dec_size = read_dword_le(input, &readoff);

    bitscnt = 1;

    while (readoff < enc_size)
    {
        bit = read_cmd_bit(input, &readoff, &bitscnt, &cmd);

        if (bit == 1) // pack: 1 byte; write: 1 byte
        {
            b = read_byte(input, &readoff);

            write_byte(output, &writeoff, b);
            write_to_wnd(window, &wndoff, b);

            //printf("off: %x; reps: %d\n", readoff, 1);
            //printf("off: %d; reps: %d; from: %d\n", readoff, 1, 0);
        }
        else
        {
            read_token(input, &readoff, &reps, &from);
            //printf("off: %x; reps: %d\n", readoff, reps);
            //printf("off: %d; reps: %d; from: %d\n", readoff, reps, from);

            while (reps-- > 0)
            {
                b = read_wnd_byte(window, &from);

                write_byte(output, &writeoff, b);
                write_to_wnd(window, &wndoff, b);
            }
        }
    }

    free(window);
    return writeoff;
}

int ADDCALL compressed_size(byte *input)
{
    int enc_size = 0, dec_size = 0, readoff = 0;
    byte bit = 0, bitscnt = 0, cmd = 0, reps = 0;
    ushort from = 0;

    readoff = 0;
    enc_size = read_dword_le(input, &readoff) + 8;
    dec_size = read_dword_le(input, &readoff);

    bitscnt = 1;

    while (readoff < enc_size)
    {
        bit = read_cmd_bit(input, &readoff, &bitscnt, &cmd);

        if (bit == 1) // pack: 1 byte; write: 1 byte
        {
            readoff++;
        }
        else
        {
            read_token(input, &readoff, &reps, &from);
        }
    }

    return readoff;
}

int main(int argc, char *argv[])
{
    byte *input, *output;

    if (argc < 4 || (argc == 4 && argv[3][0] == 'd'))
    {
        puts("usage:\n"
             "  decompression: lztoshio.exe InFilename OutFilename d HexOffset\n"
             "  compression  : lztoshio.exe InFilename OutFilename c"
        );
        return 1;
    }

    FILE *inf = fopen(argv[1], "rb");

    input = (byte *)malloc(0x10000);
    output = (byte *)malloc(0x10000);

    char mode = (argv[3][0]);

    if (mode == 'd')
    {
        long offset = strtol(argv[4], NULL, 16);
        fseek(inf, offset, SEEK_SET);
    }

    fread(&input[0], 1, 0x10000, inf);

    int dest_size;
    if (mode == 'd')
    {
        dest_size = decompress(input, output);
    }
    else
    {
        fseek(inf, 0, SEEK_END);
        int dec_size = ftell(inf);

        dest_size = compress(input, output, dec_size);
    }

    if (dest_size != 0)
    {
        FILE *outf = fopen(argv[2], "wb");
        fwrite(&output[0], 1, dest_size, outf);
        fclose(outf);
    }

    fclose(inf);

    free(input);
    free(output);

    return 0;
}

