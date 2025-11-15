
mod cm {
	mod cm_extern {
		unsafe extern "C" {
			pub fn random() -> f64;
		}
	}

	pub fn random() -> f64 {
        unsafe { cm_extern::random() }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn add(left: u64, right: u64) -> u64 {
    left + right
}

#[unsafe(no_mangle)]
pub extern "C" fn randomInt() -> i64 {
    return cm::random() as i64;
}

const _: () = {
    #[unsafe(link_section = "surmsection")]
	#[allow(dead_code)]
    static SECTION_CONTENT: [u8; 11] = *b"hello world";
};
