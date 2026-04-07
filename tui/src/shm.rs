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
}
