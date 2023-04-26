#include <stdlib.h>
#include <stdio.h>
#include <lz4.h>
#include <fstream>
#include <regex>

FILE* open_or_exit(const char* fname, const char* mode)
{
    FILE* f = fopen(fname, mode);
    if (f == NULL) {
        perror(fname);
        exit(EXIT_FAILURE);
    }
    return f;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "USAGE: %s {source} {dest}\n\n"
            "  Creates {dest}.c from the contents of {source}\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    std::string input_filename = argv[1];
    std::string output_filename = std::string(argv[2]) + ".c";
    std::string filtered_name = std::string(argv[2]);
    std::regex reg("[\\. \-]+");
    filtered_name = std::regex_replace(filtered_name, reg, "_");

    printf("Embedder: opening file %s ...\n", input_filename.c_str());
    std::ifstream input_file(input_filename, std::ios::binary);
    if (input_file.fail()) {
        printf("Failed to open file\n");
        return EXIT_FAILURE;
    }
    printf("Embedder: opened file.\n");

    // Start writing base output file
    FILE* out = open_or_exit(output_filename.c_str(), "w");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "const char %s[] = {", filtered_name.c_str());

    int file_size;
    char* inbuf;
    char* outbuf;

    // Get filesize
    input_file.seekg(0, std::ios::end);
    file_size = (int)input_file.tellg();

    // Create buffers
    inbuf = new char[file_size];
    outbuf = new char[file_size];

    // Go back to the start of the file and fill input buffer
    input_file.seekg(0, std::ios::beg);
    input_file.read(inbuf, file_size);

    // Copress
    printf("Compressing...\n");
    int compressedSize = LZ4_compress_default(inbuf, outbuf, file_size, file_size);
    if (compressedSize <= 0) {
        printf("Compression failure\n");
        return EXIT_FAILURE;
    }

    printf("Compressed size is %d. Writing...\n", compressedSize);
    for (unsigned int i = 0; i < compressedSize; ++i)  {
        fprintf(out, "%d,", outbuf[i]);
    }
    fprintf(out, "};\n");
    fprintf(out, "const size_t %s_len = sizeof(%s);\n", filtered_name.c_str(), filtered_name.c_str());
    fprintf(out, "const size_t %s_orig_len = %d;\n\n", filtered_name.c_str(), file_size);

    fclose(out);

    printf("Embedder: Finished.\n");

    return EXIT_SUCCESS;
}