#ifndef vtd_loader_h_INCLUDED
#define vtd_loader_h_INCLUDED

enum {
    VTD_undefined  = 0,
    VTD_grey       = 1,
    VTD_grey_alpha = 2,
    VTD_rgb        = 3,
    VTD_rgb_alpha  = 4
};

typedef struct {
    uint8_t channels;
    uint32_t width;
    uint32_t height;

    uint8_t *pixels;
} VtdData;

#ifdef VTD_LOADER_IMPLEMENTATION

void saveVtd(const char *filename, VtdData *image)
{
    FILE *fp = fopen(filename, "wb");

    fwrite(&image->channels, sizeof(uint8_t), 1, fp);
    fwrite(&image->width, sizeof(uint32_t), 1, fp);
    fwrite(&image->height, sizeof(uint32_t), 1, fp);

    fwrite(image->pixels, image->width * image->height, image->channels * sizeof(uint8_t), fp);

    fclose(fp);
}

void loadVtd(const char *data, size_t dataLen, VtdData *image)
{
    size_t offset = 0;

    image->channels = *((uint8_t*) (data + offset));
    offset += sizeof(uint8_t);

    image->width = *((uint32_t*) (data + offset));
    offset += sizeof(uint32_t);

    image->height = *((uint32_t*) (data + offset));
    offset += sizeof(uint32_t);

    size_t pixelSize = image->channels * image->width * image->height * sizeof(uint8_t);
    if (pixelSize + offset != dataLen) {
        fprintf(stderr, "Error parsing vtd image: image metadata doesn't match file size\n");
        image->pixels = NULL;
        return;
    }

    image->pixels = malloc(pixelSize);

    memcpy(image->pixels, data + offset, pixelSize);
}

void vtdConvert(VtdData *image, uint8_t newChannels)
{
    if (image->channels == newChannels)
        return;

    uint8_t *newPixels = malloc(newChannels * image->width * image->height * sizeof(uint8_t));

    if (image->channels > newChannels) {
        // Removing channels from image
        for (size_t i = 0; i < image->width * image->height; i++) {
            size_t oldOffset = i * image->channels * sizeof(uint8_t);
            size_t newOffset = i * newChannels * sizeof(uint8_t);
            memcpy(newPixels + newOffset, image->pixels + oldOffset, newChannels * sizeof(uint8_t));
        }
    } else {
        // Adding channels to image
        for (size_t i = 0; i < image->width * image->height; i++) {
            size_t oldOffset = i * image->channels * sizeof(uint8_t);
            size_t newOffset = i * newChannels * sizeof(uint8_t);
            memcpy(newPixels + newOffset, image->pixels + oldOffset, image->channels * sizeof(uint8_t));
            // If adding rgb channels, set it to 0
            for (size_t j = image->channels; j < newChannels; j++) {
                newPixels[i * newChannels + j] = 0;
            }
            // If adding alpha channel, set it to 255
            if (newChannels == VTD_grey_alpha || newChannels == VTD_rgb_alpha)
                newPixels[i * newChannels + newChannels - 1] = 255;
        }
    }

    free(image->pixels);
    image->pixels = newPixels;
    image->channels = newChannels;
}

void vtdFree(VtdData *image)
{
    free(image->pixels);
}

#endif // VTD_LOADER_IMPLEMENTATION

#endif // vtd_loader_h_INCLUDED
