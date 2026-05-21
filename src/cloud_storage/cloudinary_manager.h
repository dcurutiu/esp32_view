#ifndef CLOUDINARY_MANAGER_H
#define CLOUDINARY_MANAGER_H

#include "../camera/acquisition_manager.h"

struct CloudinaryManager
{
    static void uploadImage(const CapturedImage &img);
};

#endif // CLOUDINARY_MANAGER_H
