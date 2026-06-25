// Comment

enum Animal {
    Dog,
    Cat
}

struct Point {
    x: i32,
    y: i32
}

union MyUnion {
    f1: u32,
    f2: f32,
}

macro_rules! hello_world_macro {
    () => (
        println!("Hello World!");
    )
}

fn main() {
    let _i8: i8 = 1i8;
    let _i16: i16 = 1i16;
    let _i32: i32 = 1i32;
    let _i64: i64 = 1i64;
    let _isize: isize = 1isize;

    let _u8: u8 = 1u8;
    let _u16: u16 = 1u16;
    let _u32: u32 = 1u32;
    let _u64: u64 = 1u64;
    let _usize: usize = 1usize;

    let _f32: f32 = 3.14f32;
    let _f64: f64 = 3.14f64;

    let valid_binary = 0b0_10;
    let invalid_binary= 0b0_13;

    let valid_octal = 0o3_45;
    let invalid_octal = 0o3_49;

    let valid_hexadecimal = 0x9_AF;
    let invalid_hexadecimal = 0x9_AZ;

    let valid_char: char = 'a';
    let invalid_char = 'ab';

    let valid_byte: u8 = b'a';
    let invalid_byte = b'ab';

    hello_world_macro!();
}
