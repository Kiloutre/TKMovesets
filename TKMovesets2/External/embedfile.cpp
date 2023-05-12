#include <stdlib.h>
#include <stdio.h>
#include <lz4.h>
#include <fstream>
#include <regex>

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

    auto lastSlash = filtered_name.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        filtered_name.erase(0, lastSlash + 1);
    }
    std::regex reg("[\\. \\-]+");
    filtered_name = std::regex_replace(filtered_name, reg, "_");

    printf("Embedder: opening file %s ...\n", input_filename.c_str());
    std::ifstream input_file(input_filename, std::ios::binary);
    if (input_file.fail()) {
        printf("Failed to open file\n");
        return EXIT_FAILURE;
    }
    printf("Embedder: opened file.\n");

    // Start writing base output file
    std::ofstream output_file(output_filename);
    if (output_file.fail()) {
        return EXIT_FAILURE;
    }
    output_file << "#include <stdlib.h>\n";
    output_file << "const char " << filtered_name << "[] = {";

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

    printf("Compressed size is %d, old size is %d, ratio is %.2f%% . Writing...\n", compressedSize, file_size, (float)compressedSize / (float)file_size);
    for (int i = 0; i < compressedSize; ++i)  {
#pragma warning(suppress:)
        output_file << std::to_string(outbuf[i]) << ",";
    }
    output_file << "};\n";
    output_file << "const size_t " << filtered_name << "_len = sizeof(" << filtered_name << "); \n";
    output_file << "const size_t " << filtered_name << "_orig_len = " << file_size << "; \n";

    printf("Embedder: Finished.\n");

    return EXIT_SUCCESS;
}