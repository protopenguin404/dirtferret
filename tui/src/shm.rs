use std::ffi::CString;
use libc::{
    shm_open,
    mmap,
    munmap,
    ftruncate,
    PROT_WRITE,
    O_CREAT,
    O_RDWR,
    O_RDONLY,
    MAP_SHARED,
    PROT_READ,
    MAP_FAILED,
};
use std::slice::{
    from_raw_parts,
    from_raw_parts_mut,
};

pub struct ShmReader {
    ptr: *mut std::ffi::c_void,
    pub len: usize,
}

unsafe impl Send for ShmReader {}

impl ShmReader {
    pub fn open(name: &str, size: usize) -> anyhow::Result<Self> {
        let name_cstr = CString::new(name)?;

        let fd = unsafe { shm_open(name_cstr.as_ptr(), O_RDONLY, 0) };
        if fd == -1 {
            return Err(anyhow::anyhow!("shm_open failed: {}", std::io::Error::last_os_error()));
        }

        let ptr = unsafe {
            mmap(
                std::ptr::null_mut(),
                size,
                PROT_READ,
                MAP_SHARED,
                fd,
                0,
            )
        };

        unsafe { libc::close(fd); }

        if ptr == MAP_FAILED {
            return Err(anyhow::anyhow!("mmap failed: {}", std::io::Error::last_os_error()));
        }

        Ok(ShmReader { ptr, len: size })
    }

    pub fn as_bytes(&self) -> &[u8] {
        unsafe { from_raw_parts(self.ptr as *const u8, self.len) }
    }
}

impl Drop for ShmReader {
    fn drop(&mut self) {
        unsafe { munmap(self.ptr, self.len); }
    }
}

pub struct ShmWriter {
    ptr: *mut std::ffi::c_void,
    len: usize,
}

unsafe impl Send for ShmWriter {}

impl ShmWriter {
    pub fn create(name: &str, size: usize) -> anyhow::Result<Self> {
        let name_cstr = CString::new(name)?;

        let fd = unsafe { shm_open(name_cstr.as_ptr(), O_CREAT | O_RDWR, 0o600) };
        if fd == -1 {
            return Err(anyhow::anyhow!("shm_open failed: {}", std::io::Error::last_os_error()));
        }

        if unsafe { ftruncate(fd, size as i64) } == -1 {
            unsafe { libc::close(fd); }
            return Err(anyhow::anyhow!("ftruncate failed: {}", std::io::Error::last_os_error()));
        }

        let ptr = unsafe { mmap(std::ptr::null_mut(), size, PROT_WRITE, MAP_SHARED, fd, 0) };

        unsafe { libc::close(fd); }

        if ptr == MAP_FAILED {
            return Err(anyhow::anyhow!("mmap failed: {}", std::io::Error::last_os_error()));
        }

        Ok(ShmWriter { ptr, len: size })
    }

    pub fn write_bytes(&mut self, data: &[u8]) {
        let dest = unsafe { from_raw_parts_mut(self.ptr as *mut u8, self.len) };
        let n = data.len().min(self.len);
        dest[..n].copy_from_slice(&data[..n]);
    }
}

impl Drop for ShmWriter {
    fn drop(&mut self) {
        unsafe { munmap(self.ptr, self.len); }
    }
}

/// Matches the C++ FrameNotifyHeader layout exactly (280 bytes).
/// Used only to define the size — we read fields by offset.
#[repr(C)]
pub struct FrameNotifyHeader {
    pub sequence: u64,
    pub width: u32,
    pub height: u32,
    pub read_slot: u32,
    pub dirty_count: u32,
    pub dirty_rects_raw: [[u8; 16]; 16], // 16 rects × (i32 x, i32 y, u32 w, u32 h)
}

/// Parsed frame notification from a poll().
pub struct FrameHeader {
    pub width: u32,
    pub height: u32,
    pub read_slot: u32,
    pub dirty_rects: Vec<[u32; 4]>, // [x, y, w, h]
}

/// Polls a per-buffer notify shm header for new frames.
pub struct FrameNotify {
    ptr: *const u8,
    len: usize,
    last_seq: u64,
}

unsafe impl Send for FrameNotify {}

impl FrameNotify {
    pub fn open(name: &str) -> anyhow::Result<Self> {
        let size = std::mem::size_of::<FrameNotifyHeader>();
        let name_cstr = CString::new(name)?;

        let fd = unsafe { shm_open(name_cstr.as_ptr(), O_RDONLY, 0) };
        if fd == -1 {
            return Err(anyhow::anyhow!(
                "shm_open notify failed: {}",
                std::io::Error::last_os_error()
            ));
        }

        let ptr = unsafe {
            mmap(
                std::ptr::null_mut(),
                size,
                PROT_READ,
                MAP_SHARED,
                fd,
                0,
            )
        };
        unsafe { libc::close(fd); }

        if ptr == MAP_FAILED {
            return Err(anyhow::anyhow!(
                "mmap notify failed: {}",
                std::io::Error::last_os_error()
            ));
        }

        Ok(FrameNotify {
            ptr: ptr as *const u8,
            len: size,
            last_seq: 0,
        })
    }

    /// Check for a new frame. Returns Some(header) if sequence advanced.
    pub fn poll(&mut self) -> Option<FrameHeader> {
        let seq = unsafe {
            let seq_ptr = self.ptr as *const std::sync::atomic::AtomicU64;
            (*seq_ptr).load(std::sync::atomic::Ordering::Acquire)
        };

        if seq == self.last_seq {
            return None;
        }
        self.last_seq = seq;

        unsafe { Some(self.read_header()) }
    }

    unsafe fn read_header(&self) -> FrameHeader {
        let p = self.ptr;

        let width = u32::from_ne_bytes([*p.add(8), *p.add(9), *p.add(10), *p.add(11)]);
        let height = u32::from_ne_bytes([*p.add(12), *p.add(13), *p.add(14), *p.add(15)]);
        let read_slot = u32::from_ne_bytes([*p.add(16), *p.add(17), *p.add(18), *p.add(19)]);
        let dirty_count = u32::from_ne_bytes([*p.add(20), *p.add(21), *p.add(22), *p.add(23)]);

        let count = (dirty_count as usize).min(16);
        let mut dirty_rects = Vec::with_capacity(count);
        for i in 0..count {
            let base = 24 + i * 16;
            let x = i32::from_ne_bytes([*p.add(base), *p.add(base+1), *p.add(base+2), *p.add(base+3)]);
            let y = i32::from_ne_bytes([*p.add(base+4), *p.add(base+5), *p.add(base+6), *p.add(base+7)]);
            let w = u32::from_ne_bytes([*p.add(base+8), *p.add(base+9), *p.add(base+10), *p.add(base+11)]);
            let h = u32::from_ne_bytes([*p.add(base+12), *p.add(base+13), *p.add(base+14), *p.add(base+15)]);
            dirty_rects.push([x.max(0) as u32, y.max(0) as u32, w, h]);
        }

        FrameHeader {
            width,
            height,
            read_slot,
            dirty_rects,
        }
    }
}

impl Drop for FrameNotify {
    fn drop(&mut self) {
        unsafe { munmap(self.ptr as *mut std::ffi::c_void, self.len); }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shm_open() {
        let name = "/dirtferret-0";
        let phrase = b"Gobbeldy Gook!!";
        let size = phrase.len();

        let mut writer = ShmWriter::create(name, size).unwrap();
        writer.write_bytes(phrase);

        let reader = ShmReader::open(name, size).unwrap();

        assert_eq!(reader.as_bytes(), phrase)
    }

    #[test]
    fn test_frame_notify_poll_no_update() {
        let name = "/dirtferret-test-notify";
        let size = std::mem::size_of::<FrameNotifyHeader>();
        let mut writer = ShmWriter::create(name, size).unwrap();
        writer.write_bytes(&vec![0u8; size]);

        let mut notify = FrameNotify::open(name).unwrap();
        // First poll: sequence is 0, last_seq is 0 → no update
        assert!(notify.poll().is_none());

        // Clean up
        let c = std::ffi::CString::new(name).unwrap();
        unsafe { libc::shm_unlink(c.as_ptr()); }
    }

    #[test]
    fn test_frame_notify_poll_sees_update() {
        let name = "/dirtferret-test-notify2";
        let size = std::mem::size_of::<FrameNotifyHeader>();

        let mut data = vec![0u8; size];
        // sequence = 1 at offset 0
        data[0..8].copy_from_slice(&1u64.to_ne_bytes());
        // width = 800 at offset 8
        data[8..12].copy_from_slice(&800u32.to_ne_bytes());
        // height = 600 at offset 12
        data[12..16].copy_from_slice(&600u32.to_ne_bytes());
        // read_slot = 1 at offset 16
        data[16..20].copy_from_slice(&1u32.to_ne_bytes());
        // dirty_count = 1 at offset 20
        data[20..24].copy_from_slice(&1u32.to_ne_bytes());
        // dirty_rect[0]: x=10, y=20, w=100, h=200 at offset 24
        data[24..28].copy_from_slice(&10i32.to_ne_bytes());
        data[28..32].copy_from_slice(&20i32.to_ne_bytes());
        data[32..36].copy_from_slice(&100u32.to_ne_bytes());
        data[36..40].copy_from_slice(&200u32.to_ne_bytes());

        let mut writer = ShmWriter::create(name, size).unwrap();
        writer.write_bytes(&data);

        let mut notify = FrameNotify::open(name).unwrap();
        let header = notify.poll().expect("should see sequence=1");

        assert_eq!(header.width, 800);
        assert_eq!(header.height, 600);
        assert_eq!(header.read_slot, 1);
        assert_eq!(header.dirty_rects.len(), 1);
        assert_eq!(header.dirty_rects[0], [10u32, 20, 100, 200]);

        // Second poll with same sequence → None
        assert!(notify.poll().is_none());

        let c = std::ffi::CString::new(name).unwrap();
        unsafe { libc::shm_unlink(c.as_ptr()); }
    }
}
