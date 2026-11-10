#include "Camera.h"

// Thin C ABI over the bundled litecam Camera class so Rust can drive it.
// The Camera object and its frame buffers live entirely on the calling thread.

struct CamHandle
{
    Camera cam;
    FrameData last;
    bool has_last;
};

extern "C"
{
    void *litecam_create()
    {
        CamHandle *h = new CamHandle();
        h->last.rgbData = nullptr;
        h->last.width = 0;
        h->last.height = 0;
        h->last.size = 0;
        h->has_last = false;
        return h;
    }

    int litecam_open(void *handle, int index)
    {
        CamHandle *h = (CamHandle *)handle;
        return h->cam.Open(index) ? 1 : 0;
    }

    // Returns a pointer to the current RGB frame (owned by the handle) and fills
    // width/height/size. The pointer stays valid until litecam_release or the
    // next litecam_grab. Returns NULL when no frame is available.
    unsigned char *litecam_grab(void *handle, int *width, int *height, int *size)
    {
        CamHandle *h = (CamHandle *)handle;
        if (h->has_last)
        {
            ReleaseFrame(h->last);
            h->has_last = false;
        }
        FrameData f = h->cam.CaptureFrame();
        if (!f.rgbData || f.width <= 0 || f.height <= 0)
        {
            return nullptr;
        }
        h->last = f;
        h->has_last = true;
        *width = f.width;
        *height = f.height;
        *size = (int)f.size;
        return f.rgbData;
    }

    void litecam_release(void *handle)
    {
        CamHandle *h = (CamHandle *)handle;
        if (h->has_last)
        {
            ReleaseFrame(h->last);
            h->has_last = false;
        }
    }

    void litecam_destroy(void *handle)
    {
        CamHandle *h = (CamHandle *)handle;
        if (h->has_last)
        {
            ReleaseFrame(h->last);
        }
        delete h;
    }
}