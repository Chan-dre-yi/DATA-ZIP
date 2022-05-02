#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_LIMIT 256 
// maximum size of the buffer kept 256 cause thats the amount of english characters
#define FILE_END -1
#define MEM_ALLOC_FAIL -1
int num_alpha = 256;
int num_active = 0;
int *frequen = NULL;
unsigned int orig_size = 0;
typedef struct {
 int index;
 unsigned int weight;
} node_t;
node_t *nodes = NULL;
int num_nodes = 0;
int *leaf_index = NULL;
int *parent_index = NULL;
int free_index = 1;
int *stack;
int stack_top;
unsigned char buffer[MAX_LIMIT];
int bits_in_buffer = 0;
int current_bit = 0;
int eof_input = 0;
int read_header(FILE *f);
int write_header(FILE *f);
int read_bit(FILE *f);
int write_bit(FILE *f, int bit);
int flush_buffer(FILE *f);
void decode_bit_stream(FILE *fin, FILE *fout);
int decode(const char* ifile, const char *ofile);
void encode_alphabet(FILE *fout, int character);
int encode(const char* ifile, const char *ofile);
void build_tree();
void add_leaves();
int add_node(int index, int weight);
void initialise();
void det_freq(FILE *f) {
 int c;
 while ((c = fgetc(f)) != EOF) {
 ++frequen[c];
 ++orig_size;
 }
 for (c = 0; c < num_alpha; ++c)
 if (frequen[c] > 0)
 ++num_active;
}
void initialise() {
 frequen = (int *)calloc(2 * num_alpha, sizeof(int));
 leaf_index = frequen + num_alpha - 1;
}
void allocate_tree() {
 nodes = (node_t *)calloc(2 * num_active, sizeof(node_t));
 parent_index = (int *)calloc(num_active, sizeof(int));
}
int add_node(int index, int weight) {
 int i = num_nodes++;
 while (i > 0 && nodes[i].weight > weight) {
 memcpy(&nodes[i + 1], &nodes[i], sizeof(node_t));
 if (nodes[i].index < 0)
 ++leaf_index[-nodes[i].index];
 else
 ++parent_index[nodes[i].index];
 --i;
 }
 ++i;
 nodes[i].index = index;
 nodes[i].weight = weight;
 if (index < 0)
 leaf_index[-index] = i;
 else
 parent_index[index] = i;
 return i;
}
void add_leaves() {
 int i, freq;
 for (i = 0; i < num_alpha; ++i) {
 freq = frequen[i];
 if (freq > 0)
 add_node(-(i + 1), freq);
 }
}
void build_tree() {
 int a, b, index;
 while (free_index < num_nodes) {
 a = free_index++;
 b = free_index++;
 index = add_node(b/2,
 nodes[a].weight + nodes[b].weight);
 parent_index[b/2] = index;
 }
}
int encode(const char* ifile, const char *ofile) {
 FILE *fin, *fout;
 fin = fopen(ifile, "rb");
 fout = fopen(ofile, "wb");
 det_freq(fin);
 stack = (int *) calloc(num_active - 1, sizeof(int));
 allocate_tree();
 add_leaves();
 write_header(fout);
 build_tree();
 fseek(fin, 0, SEEK_SET);
 int c;
 while ((c = fgetc(fin)) != EOF)
 encode_alphabet(fout, c);
 flush_buffer(fout);
 free(stack);
 fclose(fin);
 fclose(fout);
 return 0;
}
void encode_alphabet(FILE *fout, int character) {
 int node_index;
 stack_top = 0;
 node_index = leaf_index[character + 1];
 while (node_index < num_nodes) {
 stack[stack_top++] = node_index % 2;
 node_index = parent_index[(node_index + 1) / 2];
 }
 while (--stack_top > -1)
 write_bit(fout, stack[stack_top]);
}
int decode(const char* ifile, const char *ofile) {
 FILE *fin, *fout;
 fin = fopen(ifile, "rb");
 fout = fopen(ofile, "wb");
 if (read_header(fin) == 0) {
 build_tree();
 decode_bit_stream(fin, fout);
 }
 fclose(fin);
 fclose(fout);
 return 0;
}
void decode_bit_stream(FILE *fin, FILE *fout) {
 int i = 0, bit, node_index = nodes[num_nodes].index;
 while (1) {
 bit = read_bit(fin);
 if (bit == -1)
 break;
 node_index = nodes[node_index * 2 - bit].index;
 if (node_index < 0) {
 char c = -node_index - 1;
 fwrite(&c, 1, 1, fout);
 if (++i == orig_size)
 break;
 node_index = nodes[num_nodes].index;
 }
 }
}
int write_bit(FILE *f, int bit) {
 if (bits_in_buffer == MAX_LIMIT << 3) {
 size_t bytes_written =
 fwrite(buffer, 1, MAX_LIMIT, f);
 bits_in_buffer = 0;
 memset(buffer, 0, MAX_LIMIT);
 }
 if (bit)
 buffer[bits_in_buffer >> 3] |=
 (0x1 << (7 - bits_in_buffer % 8));
 ++bits_in_buffer;
 return 0;
}
int flush_buffer(FILE *f) {
 if (bits_in_buffer) {
 size_t bytes_written =
 fwrite(buffer, 1,
 (bits_in_buffer + 7) >> 3, f);
 if (bytes_written < MAX_LIMIT && ferror(f))
 return -1;
 bits_in_buffer = 0;
 }
 return 0;
}
int read_bit(FILE *f) {
 if (current_bit == bits_in_buffer) {
 if (eof_input)
 return FILE_END;
 else {
 size_t bytes_read =
 fread(buffer, 1, MAX_LIMIT, f);
 if (bytes_read < MAX_LIMIT) {
 if (feof(f))
 eof_input = 1;
 }
 bits_in_buffer = bytes_read << 3;
 current_bit = 0;
 }
 }
 if (bits_in_buffer == 0)
 return FILE_END;
 int bit = (buffer[current_bit >> 3] >>
 (7 - current_bit % 8)) & 0x1;
 ++current_bit;
 return bit;
}
int write_header(FILE *f) {
 int i, j, byte = 0,
 size = sizeof(unsigned int) + 1 + num_active * (1 + 
sizeof(int));
 unsigned int weight;
 char *buffer = (char *) calloc(size, 1);
 if (buffer == NULL)
 return MEM_ALLOC_FAIL;
 j = sizeof(int);
 while (j--)
 buffer[byte++] =
 (orig_size >> (j << 3)) & 0xff;
 buffer[byte++] = (char) num_active;
 for (i = 1; i <= num_active; ++i) {
 weight = nodes[i].weight;
 buffer[byte++] =
 (char) (-nodes[i].index - 1);
 j = sizeof(int);
 while (j--)
 buffer[byte++] =
 (weight >> (j << 3)) & 0xff;
 }
 fwrite(buffer, 1, size, f);
 free(buffer);
 return 0;
}
int read_header(FILE *f) {
 int i, j, byte = 0, size;
 size_t bytes_read;
 unsigned char buff[4];
 bytes_read = fread(&buff, 1, sizeof(int), f);
 if (bytes_read < 1)
 return FILE_END;
 byte = 0;
 orig_size = buff[byte++];
 while (byte < sizeof(int))
 orig_size = (orig_size << (1 << 3)) | buff[byte++];
 bytes_read = fread(&num_active, 1, 1, f);
 if (bytes_read < 1)
 return FILE_END;
 
 allocate_tree();
 size = num_active * (1 + sizeof(int));
 unsigned int weight;
 char *buffer = (char *) calloc(size, 1);
 if (buffer == NULL)
 return MEM_ALLOC_FAIL;
 fread(buffer, 1, size, f);
 byte = 0;
 for (i = 1; i <= num_active; ++i) {
 nodes[i].index = -(buffer[byte++] + 1);
 j = 0;
 weight = (unsigned char) buffer[byte++];
 while (++j < sizeof(int)) {
 weight = (weight << (1 << 3)) |
 (unsigned char) buffer[byte++];
 }
 nodes[i].weight = weight;
 }
 num_nodes = (int) num_active;
 free(buffer);
 return 0;
}
int main() {
 initialise();
 int loop_var = 0;
 printf("Huffman Coding Project by Venkateswara (19BDS0007), Chandreyi (19MIY0031) and Nikita (19BEC0666)\n\n");
 printf("Please select your one of the following :\n");
 printf("1. Encode \n2. Decode \n3. Exit\n");
 scanf("%d" , &loop_var);
 if (loop_var == 1){
 char source[40] , destination[40];
 printf("Please Enter the name of the file you want to encode :\n");
 scanf("%s" , &source);
 printf("Please Enter the name of the file you want to store the encoded file in :\n");
 scanf("%s" , &destination);
encode(source, destination);
}else if (loop_var == 2){
 char source[40] , destination[40];
 printf("Please Enter the name of the file you want to dencode :\n");
 scanf("%s" , &source);
 printf("Please Enter the name of the file you want to store the decoded file in :\n");
 scanf("%s" , &destination);
decode(source, destination);
}
 free(parent_index);
 free(frequen);
 free(nodes);
 return 0;
}

