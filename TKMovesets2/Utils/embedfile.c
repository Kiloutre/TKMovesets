#include <stdlib.h>
#include <stdio.h>

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
        fprintf(stderr, "USAGE: %s {sym} {rsrc}\n\n"
            "  Creates {sym}.c from the contents of {rsrc}\n",
            argv[0]);
        return EXIT_FAILURE;
    }

    printf("Embedder: opening file %s ...\n", argv[2]);
    const char* sym = argv[1];
    FILE* in = open_or_exit(argv[2], "rb");
    printf("Embedder: opened file.\n", argv[2]);

    char symfile[256];
    snprintf(symfile, sizeof(symfile), "%s.c", sym);

    FILE* out = open_or_exit(symfile, "w");
    fprintf(out, "#include <stdlib.h>\n");
    fprintf(out, "const char %s[] = {\n", sym);

    unsigned char buf[256];
    size_t nread = 0;
    do {
        nread = fread(buf, 1, sizeof(buf), in);
        size_t i;
        for (i = 0; i < nread; i++) {
            fprintf(out, "%d,", buf[i]);
        }
    } while (nread > 0);
    fprintf(out, "};\n");
    fprintf(out, "const size_t %s_len = sizeof(%s);\n\n", sym, sym);

    fclose(in);
    fclose(out);

    printf("Embedder: Finished.\n");

    return EXIT_SUCCESS;
}