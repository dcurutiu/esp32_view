#ifndef CLOUDINARY_MANAGER_H
#define CLOUDINARY_MANAGER_H

#include "../camera/acquisition_manager.h"

struct CloudinaryManager
{
    static bool uploadImage(const CapturedImage &img);
    static bool imageExists();
};

#endif // CLOUDINARY_MANAGER_H
