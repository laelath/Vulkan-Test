#ifndef vmd_loader_h_INCLUDED
#define vmd_loader_h_INCLUDED

#include <stdint.h>

typedef struct {
    uint32_t vertexCount;
    uint32_t indexCount;

    float *vertices;
    uint32_t *indices;

    char vertexMask;

    // TODO: material information
    //char *texName;
} VmdData;

#define VMD_VERTEX_NORMAL_BIT   (1 << 0)
#define VMD_VERTEX_COLOR_BIT    (1 << 1)
#define VMD_VERTEX_TEXCOORD_BIT (1 << 2)

#ifdef VMD_LOADER_IMPLEMENTATION

size_t vmdVertexComponents(VmdData *model)
{
    size_t floats = 3;
    if (model->vertexMask & VMD_VERTEX_NORMAL_BIT) floats += 3;
    if (model->vertexMask & VMD_VERTEX_COLOR_BIT) floats += 3;
    if (model->vertexMask & VMD_VERTEX_TEXCOORD_BIT) floats += 2;
    return floats;
}

size_t vmdVertexSize(VmdData *model)
{
    return vmdVertexComponents(model) * sizeof(float);
}

void vmdFree(VmdData *model)
{
    free(model->vertices);
    free(model->indices);
}

void loadVmd(VmdData *model, const char *data, size_t dataLen)
{
    model->vertexMask = data[0];
    size_t offset = 1;

    model->vertexCount = *((uint32_t*) (data + offset));
    offset += sizeof(uint32_t);

    model->indexCount = *((uint32_t*) (data + offset));
    offset += sizeof(uint32_t);

    size_t vertSize = vmdVertexSize(model);
    if (offset + model->vertexCount * vertSize + model->indexCount * sizeof(uint32_t) != dataLen) {
        fprintf(stderr, "Error parsing vmd file: File size doesn't match that indicated by file metadata\n");
        exit(4);
    }

    model->vertices = malloc(model->vertexCount * vertSize);
    model->indices  = malloc(model->indexCount  * sizeof(uint32_t));

    memcpy(model->vertices, data + offset, model->vertexCount * vertSize);
    offset += model->vertexCount * vertSize;

    memcpy(model->indices, data + offset, model->indexCount * sizeof(uint32_t));
}

void loadVmdt(VmdData *model, const char *data, size_t dataLen)
{
    char *endl = memchr(data, '\n', dataLen);
    if (endl == NULL)
        return;

    for (size_t i = 0; i < endl - data; i++) {
        if (data[i] == 'n')
            model->vertexMask |= VMD_VERTEX_NORMAL_BIT;
        else if (data[i] == 'c')
            model->vertexMask |= VMD_VERTEX_COLOR_BIT;
        else if (data[i] == 't')
            model->vertexMask |= VMD_VERTEX_TEXCOORD_BIT;
    }

    char *start = endl + 1;
    char *nextNum = start;

    size_t count = 0;
    while (*start != '\n') {
        start = memchr(start, '\n', dataLen - (start - data)) + 1;
        count += 1;
    }

    model->vertexCount = count;
    model->vertices = malloc(count * vmdVertexSize(model));

    for (size_t i = 0; i < count * vmdVertexComponents(model); i++)
        model->vertices[i] = strtof(nextNum, &nextNum);

    nextNum = start;
    start += 1;

    count = 0;
    while (start - data + 1 < dataLen) {
        start = memchr(start, '\n', dataLen - (start - data)) + 1;
        count += 3;
    }

    model->indexCount = count;
    model->indices = malloc(count * sizeof(uint32_t));

    for (size_t i = 0; i < count; i++)
        model->indices[i] = strtoul(nextNum, &nextNum, 10);
}

void saveVmd(const char *filename, VmdData *model)
{
    FILE *fp = fopen(filename, "wb");

    fwrite(&model->vertexMask, sizeof(char), 1, fp);
    fwrite(&model->vertexCount, sizeof(uint32_t), 1, fp);
    fwrite(&model->indexCount, sizeof(uint32_t), 1, fp);
    fwrite(model->vertices, model->vertexCount, vmdVertexSize(model), fp);
    fwrite(model->indices, model->indexCount, sizeof(uint32_t), fp);

    fclose(fp);
}

#endif // VMD_LOADER_IMPLEMENTATION

#endif // vmd_loader_h_INCLUDED
