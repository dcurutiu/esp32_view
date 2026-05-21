#ifndef SUPABASE_MANAGER_H
#define SUPABASE_MANAGER_H

#include "../camera/acquisition_manager.h"

struct SupabaseManager
{
    static void uploadImage(const CapturedImage &data);
};

#endif // SUPABASE_MANAGER_H