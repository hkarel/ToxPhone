import qbs
import qbs.TextFile

Product {
    name: "PackageBuild"

    Depends { name: "cpp" }
    Depends { name: "lib.sodium" }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    Probe {
        id: productProbe
        property string projectBuildDirectory: project.buildDirectory
      //property string cppstdlibPath: cppstdlib.path

        property var libs: [
            lib.sodium,
        ]

        configure: {
            var file = new TextFile(projectBuildDirectory + "/package_build_info", TextFile.WriteOnly);
            try {
                for (var n in libs) {
                    var lib = libs[n];
                    for (var i in lib.dynamicLibraries) {
                        file.writeLine(lib.libraryPath + ("/lib{0}.so*").format(lib.dynamicLibraries[i]));
                    }
                }

                //if (!cppstdlibPath.startsWith("/usr/lib", 0)) {
                //    file.writeLine(cppstdlibPath + "/" + "libstdc++.so*");
                //    file.writeLine(cppstdlibPath + "/" + "libgcc_s.so*");
                //}
            }
            finally {
                file.close();
            }
        }
    }
}
