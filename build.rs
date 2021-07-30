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
    let t1dir = Path::new("external/grok/src/lib/jp2/t1");
    let t1part1dir = Path::new("external/grok/src/lib/jp2/t1/part1");
    let p1impldir = Path::new("external/grok/src/lib/jp2/t1/part1/impl");
    let t1ojphdir = Path::new("external/grok/src/lib/jp2/t1/OJPH");
    let part15dir = Path::new("external/grok/src/lib/jp2/t1/part15");
    let t1tiledir = Path::new("external/grok/src/lib/jp2/tile");
    let t1transformdir = Path::new("external/grok/src/lib/jp2/transform");
    let t1utildir = Path::new("external/grok/src/lib/jp2/util");
    let codingdir = Path::new("external/grok/src/lib/jp2/t1/t1_ht/coding");
    let commondir = Path::new("external/grok/src/lib/jp2/t1/t1_ht/common");

    let t2dir = Path::new("external/grok/src/lib/jp2/t2");
    let point_trf = Path::new("external/grok/src/lib/jp2/point_transform");

    let highway = Path::new("external/grok/src/lib/jp2/highway");

    let utildir = Path::new("external/grok/src/lib/jp2/util");
    let taskflowdir = Path::new("external/grok/src/include");
    let cachedir = Path::new("external/grok/src/lib/jp2/cache");
    let codestreamdir = Path::new("external/grok/src/lib/jp2/codestream");
    let codestreammkdir = Path::new("external/grok/src/lib/jp2/codestream/markers");
    let plugindir = Path::new("external/grok/src/lib/jp2/plugin");

    cc.include(coredir);
     cc.include(codingdir);
     cc.include(t1dir);
     cc.include(highway);
     cc.include(point_trf);
     cc.include(t1ojphdir);
     cc.include(t1part1dir);
     cc.include(t1tiledir);
     cc.include(t1transformdir);
     cc.include(t1utildir);
     cc.include(p1impldir);
     cc.include(commondir);
     cc.include(part15dir);

     cc.include(t2dir);

     cc.include(utildir);
     cc.include(taskflowdir);
     cc.include(cachedir);
     cc.include(codestreamdir);
     cc.include(plugindir);
     cc.include(codestreammkdir);
     cc.include(plugindir);
     cc.includes(t1dir);
    cc.cpp(true);
    cc.flag("-std=c++17");

    let files = [
          "grok.cpp",
    ];
    for file in files.iter() {
        cc.file(coredir.join(file));
    }
    cc.compile("grok");

}
