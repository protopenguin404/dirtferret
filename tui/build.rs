fn main() {
    capnpc::CompilerCommand::new()
        .src_prefix("../schema")
        .file("../schema/types.capnp")
        .file("../schema/core.capnp")
        .run()
        .expect("Cap'n Proto schema compilation failed");
}
