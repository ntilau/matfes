/*
 *
 *  Portable ANSI C matlab file exporter
 *    for exporting data from C/C++ programs into Matlab format
 *
 *   By Malcolm McLean
 *
 *  Version 2.0
 *     Bug fix - file was opened in text mode "w" in version 1. Now opened
 *               in binary mode ("wb").
 *     Functions have been added to create combined .mat files
 *       including matrices, strings, and cells.
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <assert.h>


typedef struct
{
    FILE* fp;      /* stdio file pointer, do not access */
    int err;       /* IO error values, 0 for OK */
    long* cellpos; /* stack of cell positions */
    int celldepth; /* depth of cell nesting */
} MATFILE;

MATFILE* openmatfile(char* fname, int* err);
int matfile_addmatrix(MATFILE* mf, char* name, double* data, int m, int n, int transpose);
int matfile_addstring(MATFILE* mf, char* name, char* str);
int matfile_cellpush(MATFILE* mf, char* name, int m, int n);
int matfile_cellpop(MATFILE* mf);
int matfile_close(MATFILE* mf);

int writematmatrix(char* fname, char* name, double* data, int m, int n, int transpose);
int writematstrings(char* fname, char* name, char** str, int N);

#define miINT8 1
#define miUINT16 4
#define miINT32 5
#define miUINT32 6
#define miDOUBLE 9
#define miMATRIX 14

#define mxCELL_CLASS 1
#define mxCHAR_CLASS 4
#define mxDOUBLE_CLASS 6

static int round8(int N);
//static int fputdle(FILE* fp, double x);
static int fput32le(FILE* fp, int x);
static int fput16le(FILE* fp, int x);
static int fwriteieee754(double x, FILE* fp, int bigendian);


/*
  Open a mat file for writing
  Params: fname - file to open
          err - error return
            0 = OK
            -1 = out of memory
            -2 = cannot open file
            -3 = I0 error
 */
MATFILE* openmatfile(char* fname, int* err)
{
    MATFILE* answer;
    int i;
    char buff[128];
    int bufflen;
    answer = malloc(sizeof(MATFILE));
    if(!answer)
    {
        goto out_of_memory;
    }
    answer->fp = fopen(fname, "wb");
    if(!answer->fp)
    {
        goto cant_open_file;
    }
    answer->celldepth = 0;
    answer->cellpos = 0;
    sprintf(buff, "MATLAB matrix file, generated by Malcolm McLean");
    bufflen = strlen(buff);
    for(i=0; i<123; i++)
    {
        fputc(i < bufflen ? buff[i] : ' ', answer->fp);
    }
    fputc(0, answer->fp);
    fputc(0, answer->fp);
    fputc(1, answer->fp);
    fputc('I', answer->fp);
    fputc('M', answer->fp);
    if(ferror(answer->fp))
    {
        goto io_error;
    }
    answer->err = 0;
    return answer;
out_of_memory:
    free(answer);
    if(err)
    {
        *err = -1;
    }
    return 0;
cant_open_file:
    free(answer);
    if(err)
    {
        *err = -2;
    }
    return 0;
io_error:
    fclose(answer->fp);
    free(answer);
    if(err)
    {
        *err = -3;
    }
    return 0;
}

/*
  add a matrix to a mat file
  Params: mf - the mat file
          name - name of matrix (Matlab identifier)
          data - the matix data
          m - number of rows
          n - number of columns
          transpose - 0 = data is passed in as column major (C format),
                      1 = data is row major
  Returns: 0 on success, -1 on fail
 */
int matfile_addmatrix(MATFILE* mf, char* name, double* data, int m, int n, int transpose)
{
    int totalsize;
    int i, ii;
    FILE* fp;
    fp = mf->fp;
    /* the main tag */
    totalsize = 8 + 8 + strlen(name) + (n * m * 8) + 8*4;
    if(strlen(name) % 8)
    {
        totalsize += 8 - (strlen(name) % 8);
    }
    fput32le(fp, miMATRIX);
    fput32le(fp, totalsize);
    /* array descriptor field */
    fput32le(fp, miUINT32);
    fput32le(fp, 8);
    fputc(mxDOUBLE_CLASS, fp);
    fputc(4, fp); /* array flags */
    fputc(0, fp);
    fputc(0, fp);
    fput32le(fp, 0);
    /* array dimensions */
    fput32le(fp, miINT32);
    fput32le(fp, 2 * 4);
    fput32le(fp, m);
    fput32le(fp, n);
    /* array name */
    fput32le(fp, miINT8);
    fput32le(fp, strlen(name));
    for(i=0; name[i]; i++)
    {
        fputc(name[i], fp);
    }
    while(i%8)
    {
        fputc(0, fp);
        i++;
    }
    /* the actual data */
    fput32le(fp, miDOUBLE);
    fput32le(fp, m * n * 8);
    if(transpose)
    {
        for(i=0; i<m; i++)
            for(ii=0; ii<n; ii++)
            {
                fwriteieee754(data[i*n+ii], fp, 0);
            }
    }
    else
    {
        for(i=0; i<n; i++)
            for(ii=0; ii<m; ii++)
            {
                fwriteieee754(data[ii*n+i], fp, 0);
            }
    }
    if(ferror(fp))
    {
        mf->err = -1;
    }
    return ferror(fp);
}

/*
  add a string to a mat file
  Params: mf - the mat file
          name - matlab identifier for string
          str - the string. Note passed in as ascii, 8 bits per char
                stored as a 16 bit Unicode string

 */
int matfile_addstring(MATFILE* mf, char* name, char* str)
{
    int totalsize;
    int len;
    int i;
    FILE* fp;
    fp = mf->fp;
    /* matrix tag */
    totalsize = 16 + 16 + 8 + strlen(name) + 8  + round8(strlen(str) *2);
    if(strlen(name) % 8)
    {
        totalsize += 8 - (strlen(name) % 8);
    }
    fput32le(fp, miMATRIX);
    fput32le(fp, totalsize);
    /* matrix class */
    fput32le(fp, miUINT32);
    fput32le(fp, 8);
    fputc(mxCHAR_CLASS, fp);
    fputc(0, fp);
    fputc(0, fp);
    fputc(0, fp);
    fput32le(fp, 0);
    /* matrix dimensions */
    len = strlen(str);
    fput32le(fp, miUINT32);
    fput32le(fp, 8);
    fput32le(fp, 1);
    fput32le(fp, len);
    /* array name */
    fput32le(fp, miINT8);
    fput32le(fp, strlen(name));
    for(i=0; name[i]; i++)
    {
        fputc(name[i], fp);
    }
    while(i%8)
    {
        fputc(0, fp);
        i++;
    }
    /* cell length and data type */
    fput32le(fp, miUINT16);
    fput32le(fp, len * 2);
    /* the actual string */
    for(i=0; str[i]; i++)
    {
        fput16le(fp, str[i]);
    }
    while(len % 4)
    {
        fput16le(fp, 0);
        len++;
    }
    if(ferror(fp))
    {
        mf->err = -1;
    }
    return mf->err;
}


/*
  Start a matlab cell matrix
  Params: mf - the matfile object
          name - cell Matlab identifier
          m - cell columns
          n - cell rows
  Notes:
    Matlab cell variables contain mixed data. Cells can also contain
    other cell data.
    So we need to push and pop the cell stack to keep track of the cell
    nesting level.

    use
    matfile_cellpush(mf, "testcell", 3, 2);
    for(cols = 0; cols < 2; cols++)
      for(rows = 0; rows < 3; rows++)
      {
        sprintf(buff, "row %d col %d\n", rows + 1, cols +1);
        matfile_addstring(mf, "a", buff);
      }
   matfile_cellpop(mf);
 */
int matfile_cellpush(MATFILE* mf, char* name, int m, int n)
{
    FILE* fp;
    void* temp;
    int i;
    fp = mf->fp;
    temp = realloc(mf->cellpos, (mf->celldepth+1) * sizeof(long));
    if(!temp)
    {
        mf->err = -1;
        return -1;
    }
    mf->cellpos = temp;
    mf->cellpos[mf->celldepth] = ftell(fp);
    /* matrix + size */
    fput32le(fp, miMATRIX);
    fput32le(fp, 0);
    /* array descriptor field */
    fput32le(fp, miUINT32);
    fput32le(fp, 8);
    fputc(mxCELL_CLASS, fp);
    fputc(4, fp); /* array flags */
    fputc(0, fp);
    fputc(0, fp);
    fput32le(fp, 0);
    /* array dimensions */
    fput32le(fp, miINT32);
    fput32le(fp, 2 * 4);
    fput32le(fp, m);
    fput32le(fp, n);
    /* array name */
    fput32le(fp, miINT8);
    fput32le(fp, strlen(name));
    for(i=0; name[i]; i++)
    {
        fputc(name[i], fp);
    }
    while(i%8)
    {
        fputc(0, fp);
        i++;
    }
    mf->celldepth++;
    if(ferror(fp))
    {
        mf->err = -1;
    }
    return mf->err;
}

/*
  Pop the cell after writing all the cells of cell matrix
 */
int matfile_cellpop(MATFILE* mf)
{
    FILE* fp;
    long pos;
    long size;
    if(mf->celldepth <= 0)
    {
        return -1;
    }
    fp = mf->fp;
    pos = ftell(fp);
    size = pos - mf->cellpos[mf->celldepth-1] - 8;
    fseek(fp, mf->cellpos[mf->celldepth-1] + 4, SEEK_SET);
    fput32le(fp, size);
    fseek(fp, pos, SEEK_SET);
    mf->celldepth--;
    return 0;
}

/*
  close the matfile
  Returns: -1 on error
  Note you should check the return value. Errors are sticky.
 */
int matfile_close(MATFILE* mf)
{
    int answer = 0;
    if(mf)
    {
        answer = fclose(mf->fp);
        if(answer == 0)
        {
            answer = mf->err;
        }
        free(mf->cellpos);
        free(mf);
    }
    return answer;
}

/*
 * Quick and easy function to write a double array as a MATLAB matrix file.
 * Params: fname - name of file to write
 *         name - MATLAB name of variable
 *         data - the data (in C format)
 *         m - number of rows
 *         n - number of columns
 *         transpose - flag for transposing data (i.e. data is in MATLAB format)
 *  Returns: 0 on success, -1 on fail
 */
int writematmatrix(char* fname, char* name, double* data, int m, int n, int transpose)
{
    FILE* fp;
    int i, ii;
    char buff[128];
    int bufflen;
    int totalsize;
    int err;
    assert(m > 0);
    assert(n > 0);
    assert((m*n)/n == m);
    fp = fopen(fname, "wb");
    if(!fp)
    {
        return -1;
    }
    sprintf(buff, "MATLAB matrix file of %s, generated by Malcolm McLean",
            name);
    bufflen = strlen(buff);
    for(i=0; i<123; i++)
    {
        fputc(i < bufflen ? buff[i] : ' ', fp);
    }
    fputc(0, fp);
    fputc(0, fp);
    fputc(1, fp);
    fputc('I', fp);
    fputc('M', fp);
    /* the main tag */
    totalsize = 8 + 8 + strlen(name) + (n * m * 8) + 8*4;
    if(strlen(name) % 8)
    {
        totalsize += 8 - (strlen(name) % 8);
    }
    fput32le(fp, miMATRIX);
    fput32le(fp, totalsize);
    /* array descriptor field */
    fput32le(fp, miUINT32);
    fput32le(fp, 8);
    fputc(mxDOUBLE_CLASS, fp);
    fputc(4, fp); /* array flags */
    fputc(0, fp);
    fputc(0, fp);
    fput32le(fp, 0);
    /* array dimensions */
    fput32le(fp, miINT32);
    fput32le(fp, 2 * 4);
    fput32le(fp, m);
    fput32le(fp, n);
    /* array name */
    fput32le(fp, miINT8);
    fput32le(fp, strlen(name));
    for(i=0; name[i]; i++)
    {
        fputc(name[i], fp);
    }
    while(i%8)
    {
        fputc(0, fp);
        i++;
    }
    /* the actual data */
    fput32le(fp, miDOUBLE);
    fput32le(fp, m * n * 8);
    if(transpose)
    {
        for(i=0; i<m; i++)
            for(ii=0; ii<n; ii++)
            {
                fwriteieee754(data[i*n+ii], fp, 0);
            }
    }
    else
    {
        for(i=0; i<n; i++)
            for(ii=0; ii<m; ii++)
            {
                fwriteieee754(data[ii*n+i], fp, 0);
            }
    }
    if(ferror(fp))
    {
        fclose(fp);
        return -1;
    }
    err = fclose(fp);
    if(err)
    {
        return -1;
    }
    return 0;
}

int writematstrings(char* fname, char* name, char** str, int N)
{
    FILE* fp;
    int bufflen;
    int celllen;
    int len;
    char buff[128];
    int totalsize;
    int err;
    int i, ii;
    fp = fopen(fname, "wb");
    if(!fp)
    {
        return -1;
    }
    sprintf(buff, "MATLAB matrix file of %s, generated by Malcolm McLean",
            name);
    bufflen = strlen(buff);
    for(i=0; i<123; i++)
    {
        fputc(i < bufflen ? buff[i] : ' ', fp);
    }
    fputc(0, fp);
    fputc(0, fp);
    fputc(1, fp);
    fputc('I', fp);
    fputc('M', fp);
    /* the main tag */
    totalsize = 8 + 8 + 16 + 8 + round8(strlen(name));
    for(i=0; i<N; i++)
        if(str[i])
        {
            totalsize += 8 + 16 + 16 + 8 + 8 + round8(strlen(str[i]) * 2);
        }
        else
        {
            totalsize += 8;
        }
    fput32le(fp, miMATRIX);
    fput32le(fp, totalsize);
    /* array descriptor field */
    fput32le(fp, miUINT32);
    fput32le(fp, 8);
    fputc(mxCELL_CLASS, fp);
    fputc(4, fp); /* array flags */
    fputc(0, fp);
    fputc(0, fp);
    fput32le(fp, 0);
    /* array dimensions */
    fput32le(fp, miINT32);
    fput32le(fp, 2 * 4);
    fput32le(fp, 1);
    fput32le(fp, N);
    /* array name */
    fput32le(fp, miINT8);
    fput32le(fp, strlen(name));
    for(i=0; name[i]; i++)
    {
        fputc(name[i], fp);
    }
    while(i%8)
    {
        fputc(0, fp);
        i++;
    }
    /* loop over cell contents */
    for(i=0; i<N; i++)
    {
        if(str[i] == 0)
        {
            fput32le(fp, miMATRIX);
            fput32le(fp, 0);
            continue;
        }
        /* matrix tag */
        celllen = 16 + 16 + 8 + 8 + round8(strlen(str[i]) *2);
        fput32le(fp, miMATRIX);
        fput32le(fp, celllen);
        /* matrix class */
        fput32le(fp, miUINT32);
        fput32le(fp, 8);
        fputc(mxCHAR_CLASS, fp);
        fputc(0, fp);
        fputc(0, fp);
        fputc(0, fp);
        fput32le(fp, 0);
        /* matrix dimensions */
        len = strlen(str[i]);
        fput32le(fp, miUINT32);
        fput32le(fp, 8);
        fput32le(fp, 1);
        fput32le(fp, len);
        /*write dummy name */
        fput16le(fp, 1);
        fput16le(fp, miINT8);
        fputc('A', fp);
        fputc(0, fp);
        fputc(0, fp);
        fputc(0, fp);
        /* cell length and data type */
        fput32le(fp, miUINT16);
        fput32le(fp, len * 2);
        /* the actual string */
        for(ii=0; str[i][ii]; ii++)
        {
            fput16le(fp, str[i][ii]);
        }
        while(len % 4)
        {
            fput16le(fp, 0);
            len++;
        }
    }
    if(ferror(fp))
    {
        fclose(fp);
        return -1;
    }
    err = fclose(fp);
    if(err)
    {
        return -1;
    }
    return 0;
}

/*
 * rund up to the nearest mutliple of 8
 */
static int round8(int N)
{
    if(N % 8)
    {
        return N + 8 - (N % 8);
    }
    else
    {
        return N;
    }
}
/*
 * put a double to a file in little-endian format
 * this is a faster routine that works if you know that your
 * floating point unit is ieee754, but don't know the endianness.
 */
 /*
static int fputdle(FILE* fp, double x)
{
    unsigned char array[8];
    unsigned char test[8];
    int i;
    double minusone = -1;
    memcpy(array, &x, 8);
    memcpy(test, &minusone, 8);
    if(test[0] & 0x80)
    {
        for(i=0; i<8; i++)
        {
            fputc(array[8-i-1], fp);
        }
    }
    else
    {
        for(i=0; i<8; i++)
        {
            fputc(array[i], fp);
        }
    }
    return 0;
}
*/
/*
 * write a double to a stream in ieee754 format regardless of host
 *  encoding.
 *  x - number to write
 *  fp - the stream
 *  bigendian - set to write big bytes first, elee write litle bytes
 *              first
 *  Returns: 0 or EOF on error
 *  Notes: different NaN types and negative zero not preserved.
 *         if the number is too big to represent it will become infinity
 *         if it is too small to represent it will become zero.
 */
static int fwriteieee754(double x, FILE* fp, int bigendian)
{
    int shift;
    unsigned long sign, exp, hibits, hilong, lowlong;
    double fnorm, significand;
    int expbits = 11;
    int significandbits = 52;
    /* zero (can't handle signed zero) */
    if(x == 0)
    {
        hilong = 0;
        lowlong = 0;
        goto writedata;
    }
    /* infinity */
    if(x > DBL_MAX)
    {
        hilong = 1024 + ((1<<(expbits-1)) - 1);
        hilong <<= (31 - expbits);
        lowlong = 0;
        goto writedata;
    }
    /* -infinity */
    if(x < -DBL_MAX)
    {
        hilong = 1024 + ((1<<(expbits-1)) - 1);
        hilong <<= (31-expbits);
        hilong |= (1 << 31);
        lowlong = 0;
        goto writedata;
    }
    /* NaN - dodgy because many compilers optimise out this test, but
     *there is no portable isnan() */
    if(x != x)
    {
        hilong = 1024 + ((1<<(expbits-1)) - 1);
        hilong <<= (31 - expbits);
        lowlong = 1234;
        goto writedata;
    }
    /* get the sign */
    if(x < 0)
    {
        sign = 1;
        fnorm = -x;
    }
    else
    {
        sign = 0;
        fnorm = x;
    }
    /* get the normalized form of f and track the exponent */
    shift = 0;
    while(fnorm >= 2.0)
    {
        fnorm /= 2.0;
        shift++;
    }
    while(fnorm < 1.0)
    {
        fnorm *= 2.0;
        shift--;
    }
    /* check for denormalized numbers */
    if(shift < -1022)
    {
        while(shift < -1022)
        {
            fnorm /= 2.0;
            shift++;
        }
        shift = -1023;
    }
    /* out of range. Set to infinity */
    else if(shift > 1023)
    {
        hilong = 1024 + ((1<<(expbits-1)) - 1);
        hilong <<= (31-expbits);
        hilong |= (sign << 31);
        lowlong = 0;
        goto writedata;
    }
    else
    {
        fnorm = fnorm - 1.0;    /* take the significant bit off mantissa */
    }
    /* calculate the integer form of the significand */
    /* hold it in a  double for now */
    significand = fnorm * ((1LL<<significandbits) + 0.5f);
    /* get the biased exponent */
    exp = shift + ((1<<(expbits-1)) - 1); /* shift + bias */
    /* put the data into two longs (for convenience) */
    hibits = (long)(significand / 4294967296);
    hilong = (sign << 31) | (exp << (31-expbits)) | hibits;
    x = significand - hibits * 4294967296;
    lowlong = (unsigned long)(significand - hibits * 4294967296);
writedata:
    /* write the bytes out to the stream */
    if(bigendian)
    {
        fputc((hilong >> 24) & 0xFF, fp);
        fputc((hilong >> 16) & 0xFF, fp);
        fputc((hilong >> 8)  & 0xFF, fp);
        fputc(hilong & 0xFF, fp);
        fputc((lowlong >> 24) & 0xFF, fp);
        fputc((lowlong >> 16) & 0xFF, fp);
        fputc((lowlong >> 8)  & 0xFF, fp);
        fputc(lowlong & 0xFF, fp);
    }
    else
    {
        fputc(lowlong & 0xFF, fp);
        fputc((lowlong >> 8)  & 0xFF, fp);
        fputc((lowlong >> 16) & 0xFF, fp);
        fputc((lowlong >> 24) & 0xFF, fp);
        fputc(hilong & 0xFF, fp);
        fputc((hilong >> 8)  & 0xFF, fp);
        fputc((hilong >> 16) & 0xFF, fp);
        fputc((hilong >> 24) & 0xFF, fp);
    }
    return ferror(fp);
}

/*
 * put a 32 bit little-endian integer to file
 **/
static int fput32le(FILE* fp, int x)
{
    int i;
    for(i=0; i<4; i++)
    {
        fputc(x & 0xFF, fp);
        x >>= 8;
    }
    return 0;
}

/*
 * Write a 16-bit little-endian integer to file
 */
static int fput16le(FILE* fp, int x)
{
    fputc(x & 0xFF, fp);
    fputc((x >> 8) & 0xFF, fp);
    return 0;
}



/*
 *  rename this main to unit-test the functions
 */
int matfilesmain(void)
{
    double mtx[12] = { 0.1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    char* strings[] = {"My name is Fred", "My name is Jim", 0, "My Name is Harry"};
    double* array;
    int err;
    int i;
    MATFILE* mf;
    int cols, rows;
    char buff[256];
    err = writematmatrix("temp.mat", "myvar", mtx, 3, 4, 0);
    if(err)
    {
        printf("Error writing file\n");
    }
    err = writematstrings("fred.mat", "fred", strings, 4);
    if(err)
    {
        printf("Error writing file\n");
    }
    array = malloc(50 * sizeof(double));
    for(i=0; i<50; i++)
    {
        array[i] = (double) 1.0/i;
    }
    writematmatrix("temp.mat", "myvar", array, 50, 1, 0);
    mf = openmatfile("combined.mat", &err);
    if(!mf)
    {
        printf("Can't open mat file error %d\n", err);
    }
    matfile_cellpush(mf, "x", 3, 1);
    matfile_addmatrix(mf, "a", mtx, 3, 4, 0);
    matfile_addmatrix(mf, "a", mtx, 12, 1, 0);
    matfile_addstring(mf, "a", "My name is fred");
    matfile_cellpop(mf);
    matfile_cellpush(mf, "testcell", 3, 2);
    for(cols = 0; cols < 2; cols++)
        for(rows = 0; rows < 3; rows++)
        {
            if(cols == 0 && rows == 1)
            {
                matfile_cellpush(mf, "testcell", 3, 1);
                for(i=0; i<3; i++)
                {
                    matfile_addmatrix(mf, "a", mtx, i+1, 12/(i+1), 0);
                }
                matfile_cellpop(mf);
            }
            sprintf(buff, "row %d col %d\n", rows, cols);
            matfile_addstring(mf, "a", buff);
        }
    matfile_cellpop(mf);
    matfile_close(mf);
    return 0;
}
