// build.rs
use std::env;
use std::path::PathBuf;

fn main() {
    // Build C++ code
    cc::Build::new()
        .cpp(true)
        .file("src/cpp/cmake_builder.cpp")
        .file("src/cpp/abi_manager.cpp")
        .file("src/cpp/compiler_detector.cpp")
        .include("/usr/include")
        .flag("-std=c++17")
        .compile("cpppm_native");

    // Generate bindings
    let bindings = bindgen::Builder::default()
        .header("src/cpp/wrapper.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}