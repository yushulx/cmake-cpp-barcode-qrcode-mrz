use std::ffi::c_void;
use std::os::raw::c_int;

extern "C" {
    fn litecam_create() -> *mut c_void;
    fn litecam_open(handle: *mut c_void, index: c_int) -> c_int;
    fn litecam_grab(
        handle: *mut c_void,
        width: *mut c_int,
        height: *mut c_int,
        size: *mut c_int,
    ) -> *mut u8;
    fn litecam_release(handle: *mut c_void);
    fn litecam_destroy(handle: *mut c_void);
}

pub struct Frame {
    pub rgb: Vec<u8>,
    pub width: i32,
    pub height: i32,
}

/// Owns a litecam capture device. Create and use it on a single thread only.
pub struct Camera {
    handle: *mut c_void,
}

impl Camera {
    pub fn open(index: i32) -> Result<Self, String> {
        let handle = unsafe { litecam_create() };
        if handle.is_null() {
            return Err("Failed to create camera handle".to_string());
        }
        if unsafe { litecam_open(handle, index) } != 1 {
            unsafe { litecam_destroy(handle) };
            return Err(format!("No camera available at index {index}"));
        }
        Ok(Self { handle })
    }

    /// Grab one frame, copying the RGB bytes out and releasing the native buffer.
    pub fn grab(&mut self) -> Option<Frame> {
        let mut width = 0;
        let mut height = 0;
        let mut size = 0;
        let ptr = unsafe { litecam_grab(self.handle, &mut width, &mut height, &mut size) };
        if ptr.is_null() || width <= 0 || height <= 0 || size <= 0 {
            unsafe { litecam_release(self.handle) };
            return None;
        }
        let rgb = unsafe { std::slice::from_raw_parts(ptr, size as usize) }.to_vec();
        unsafe { litecam_release(self.handle) };
        Some(Frame { rgb, width, height })
    }
}

impl Drop for Camera {
    fn drop(&mut self) {
        unsafe { litecam_destroy(self.handle) };
    }
}