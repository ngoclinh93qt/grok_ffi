fn main() {
    // if std::env::var("DOCS_RS").is_ok() {
    //     return;
    // }
    // pkg_config::Config::new()
    //     .atleast_version("9.2.0")
    //     .probe("libgrokj2k")
    //     .unwrap();

         use std::path::Path;

    let mut cc = cc::Build::new();
    let coredir = Path::new("external/grok/src/lib/jp2");
    let codingdir = Path::new("external/grok/src/lib/jp2/t1/t1_ht/coding");
    let commongdir = Path::new("external/grok/src/lib/jp2/t1/t1_ht/common");
    let utildir = Path::new("external/grok/src/lib/jp2/util");
    let taskflowdir = Path::new("external/grok/src/include");
    let cachedir = Path::new("external/grok/src/lib/jp2/cache");
    let codestreamdir = Path::new("external/grok/src/lib/jp2/codestream");
    let plugindir = Path::new("external/grok/src/lib/jp2/plugin");

    cc.include(coredir);
     cc.include(codingdir);
     cc.include(commongdir);
     cc.include(utildir);
     cc.include(taskflowdir);
     cc.include(cachedir);
     cc.include(codestreamdir);
     cc.include(plugindir);
    cc.cpp(true);
    cc.cpp_link_stdlib("stdc++");
    cc.cpp_link_stdlib("libc++");

    let files = [
          "grok.cpp",
    ];
    for file in files.iter() {
        cc.file(coredir.join(file));
    }
    cc.compile("grokj2k-sys.a");

}
